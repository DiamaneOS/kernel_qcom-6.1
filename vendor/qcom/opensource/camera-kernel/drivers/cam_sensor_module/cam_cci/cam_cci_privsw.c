// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Camera floor, CCI side. Every image sensor register write goes through
 * cam_cci_core_cfg(), whichever camera subdevice sends it and whichever
 * slave address it names. While the privacy switch blocks the cameras
 * (armed at boot, switch down):
 * - writes to a known sensor's test-pattern registers reach the sensor as
 *   the forced pattern; CamX's own values are remembered;
 * - writes that would re-address a sensor, and general-call writes, are
 *   dropped (CamX sees success);
 * - after every userspace write to a known sensor the pattern is written
 *   again, so a reset or a new mode cannot clear it;
 * - on the transition every sensor in use gets the pattern at once, and when
 *   the switch moves back CamX's own values are written back.
 * Streams keep running with the same mode and timing, so CamX and apps see
 * frames, not errors. Unblocked, the filter only remembers values.
 */

#include <linux/atomic.h>
#include <linux/diamaneos_privsw.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "cam_cci_privsw.h"

static LIST_HEAD(cci_privsw_devices);
static DEFINE_MUTEX(cci_privsw_devices_lock);
/* Fail closed until the switch driver tells the state. */
static atomic_t cci_privsw_blocked = ATOMIC_INIT(1);

static struct cam_cci_privsw_slot *cci_privsw_slot(struct cci_device *cci_dev,
	enum cci_i2c_master_t master, const struct cci_privsw_sensor *s,
	bool create)
{
	struct cam_cci_privsw_slot *free = NULL;
	int i;

	lockdep_assert_held(&cci_dev->privsw_lock);
	for (i = 0; i < CAM_CCI_PRIVSW_SLOTS; i++) {
		struct cam_cci_privsw_slot *slot = &cci_dev->privsw_slots[i];

		if (slot->sensor == s && slot->master == master)
			return slot;
		if (!slot->sensor && !free)
			free = slot;
	}
	if (!create || !free)
		return NULL;
	memset(free, 0, sizeof(*free));
	free->sensor = s;
	free->master = master;
	return free;
}

static u16 cci_privsw_reg(const struct cam_sensor_i2c_reg_setting *w, u32 reg)
{
	return w->addr_type == CAMERA_SENSOR_I2C_TYPE_BYTE ? reg & 0xff : reg & 0xffff;
}

int cam_cci_privsw_filter(struct cci_device *cci_dev, struct cam_cci_ctrl *ctrl,
	struct cam_sensor_i2c_reg_array **copy)
{
	struct cam_sensor_i2c_reg_setting *w = &ctrl->cfg.cci_i2c_write_cfg;
	const struct cci_privsw_sensor *s;
	struct cam_cci_privsw_slot *slot;
	struct cci_privsw_state st;
	bool blocked = atomic_read(&cci_privsw_blocked);
	bool seq = ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ ||
		   ctrl->cmd == MSM_CCI_I2C_WRITE_BURST;
	bool dataport;
	u16 sid = ctrl->cci_info->sid;
	u16 base;
	u32 i;
	int rc = 0;

	*copy = NULL;
	if (ctrl->privsw_internal)
		return 0;
	if (sid == 0) {
		if (!blocked)
			return 0;
		CAM_WARN_RATE_LIMIT(CAM_CCI, "camera floor: general call dropped");
		return 1;
	}
	s = cci_privsw_find(sid);
	if (!s)
		return 0;
	/* Malformed writes are the core's to refuse. */
	if (!w->reg_setting || !w->size || w->size > CCI_I2C_MAX_WRITE ||
	    w->data_type < CAMERA_SENSOR_I2C_TYPE_BYTE ||
	    w->data_type > CAMERA_SENSOR_I2C_TYPE_DWORD)
		return 0;

	base = cci_privsw_reg(w, w->reg_setting[0].reg_addr);
	dataport = seq && s->samsung_indirect && base == CCI_PRIVSW_SS_DATA &&
		   w->addr_type == CAMERA_SENSOR_I2C_TYPE_WORD;

	mutex_lock(&cci_dev->privsw_lock);
	slot = cci_privsw_slot(cci_dev, ctrl->cci_info->cci_i2c_master, s, true);
	if (!slot) {
		mutex_unlock(&cci_dev->privsw_lock);
		if (!blocked)
			return 0;
		CAM_WARN_RATE_LIMIT(CAM_CCI, "camera floor: no slot for %s, write dropped",
			s->name);
		return 1;
	}
	/* Work on a copy: a refused write leaves the remembered state alone. */
	st = slot->st;
	for (i = 0; i < w->size; i++) {
		u16 reg = seq ? (u16)(base + i * w->data_type) :
				cci_privsw_reg(w, w->reg_setting[i].reg_addr);
		u32 data = w->reg_setting[i].reg_data;
		enum cci_privsw_verdict v;

		v = cci_privsw_entry(s, &st, blocked, reg, dataport,
				     w->data_type, &data);
		if (v == CCI_PRIVSW_DENY) {
			CAM_WARN_RATE_LIMIT(CAM_CCI,
				"camera floor: %s write to 0x%04x dropped", s->name, reg);
			rc = 1;
			break;
		}
		if (v == CCI_PRIVSW_CHANGED) {
			if (!*copy) {
				*copy = kmemdup(w->reg_setting,
					w->size * sizeof(*w->reg_setting), GFP_KERNEL);
				if (!*copy) {
					/* Cannot hold the pattern: send nothing. */
					rc = 1;
					break;
				}
			}
			(*copy)[i].reg_data = data;
		}
	}
	if (rc == 0) {
		slot->st = st;
		slot->client = *ctrl->cci_info;
	} else {
		kfree(*copy);
		*copy = NULL;
	}
	mutex_unlock(&cci_dev->privsw_lock);
	return rc;
}

static void cci_privsw_fill(const struct cam_cci_privsw_slot *slot, bool block,
	struct cam_cci_ctrl *out, struct cam_sensor_cci_client *client,
	struct cam_sensor_i2c_reg_array *regs)
{
	const struct cci_privsw_sensor *s = slot->sensor;
	int i;

	for (i = 0; i < s->pattern_len; i++) {
		regs[i].reg_addr = s->pattern_reg + i;
		regs[i].reg_data = block ? cci_privsw_forced(s, &slot->st, i) :
					   slot->st.shadow[i];
		regs[i].delay = 0;
		regs[i].data_mask = 0;
	}
	memset(out, 0, sizeof(*out));
	out->cci_info = client;
	out->cmd = MSM_CCI_I2C_WRITE;
	out->cfg.cci_i2c_write_cfg.reg_setting = regs;
	out->cfg.cci_i2c_write_cfg.size = s->pattern_len;
	out->cfg.cci_i2c_write_cfg.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	out->cfg.cci_i2c_write_cfg.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	out->privsw_internal = true;
}

bool cam_cci_privsw_pattern(struct cci_device *cci_dev,
	const struct cam_cci_ctrl *ctrl, struct cam_cci_ctrl *out,
	struct cam_sensor_i2c_reg_array *regs)
{
	const struct cci_privsw_sensor *s;
	struct cam_cci_privsw_slot *slot;
	bool found = false;

	if (ctrl->privsw_internal || !atomic_read(&cci_privsw_blocked))
		return false;
	s = cci_privsw_find(ctrl->cci_info->sid);
	if (!s)
		return false;
	mutex_lock(&cci_dev->privsw_lock);
	slot = cci_privsw_slot(cci_dev, ctrl->cci_info->cci_i2c_master, s, false);
	if (slot) {
		/* The writer's own client: same master, address and speed. */
		cci_privsw_fill(slot, true, out, ctrl->cci_info, regs);
		found = true;
	}
	mutex_unlock(&cci_dev->privsw_lock);
	return found;
}

/* Writes the pattern (block) or CamX's values (unblock) to every sensor in use. */
static void cci_privsw_apply(struct cci_device *cci_dev, bool block)
{
	struct cam_sensor_i2c_reg_array regs[CCI_PRIVSW_PATTERN_MAX];
	struct cam_sensor_cci_client client;
	struct cam_cci_ctrl ctrl;
	int i, rc;

	mutex_lock(&cci_dev->init_mutex);
	for (i = 0; i < CAM_CCI_PRIVSW_SLOTS; i++) {
		struct cam_cci_privsw_slot *slot = &cci_dev->privsw_slots[i];
		enum cci_i2c_master_t master;
		const char *name;

		mutex_lock(&cci_dev->privsw_lock);
		if (!slot->sensor) {
			mutex_unlock(&cci_dev->privsw_lock);
			continue;
		}
		master = slot->master;
		name = slot->sensor->name;
		client = slot->client;
		cci_privsw_fill(slot, block, &ctrl, &client, regs);
		mutex_unlock(&cci_dev->privsw_lock);

		/* Only a powered sensor on an initialised master can be written. */
		if (cci_dev->cci_state != CCI_STATE_ENABLED ||
		    !cci_dev->master_active_slave[master] ||
		    !cci_dev->cci_master_info[master].is_initilized ||
		    !client.cci_subdev)
			continue;
		rc = cam_cci_privsw_write(cci_dev, &ctrl);
		CAM_INFO(CAM_CCI, "camera floor: %s %s on CCI%d_I2C_M%d: %d", name,
			block ? "pattern on" : "restored", cci_dev->soc_info.index, master, rc);
	}
	mutex_unlock(&cci_dev->init_mutex);
}

static int cci_privsw_event(struct notifier_block *nb, unsigned long action,
	void *data)
{
	struct cci_device *cci_dev;
	bool block = action == PRIVSW_BLOCK;

	/* Blocking: filter first, then write; unblocking: stop filtering, then restore. */
	atomic_set(&cci_privsw_blocked, block);
	mutex_lock(&cci_privsw_devices_lock);
	list_for_each_entry(cci_dev, &cci_privsw_devices, privsw_node)
		cci_privsw_apply(cci_dev, block);
	mutex_unlock(&cci_privsw_devices_lock);
	CAM_INFO(CAM_CCI, "camera floor: %s", block ? "blocking" : "released");
	return NOTIFY_OK;
}

static struct notifier_block cci_privsw_nb = {
	.notifier_call = cci_privsw_event,
};

void cam_cci_privsw_add(struct cci_device *cci_dev)
{
	mutex_init(&cci_dev->privsw_lock);
	mutex_lock(&cci_privsw_devices_lock);
	list_add_tail(&cci_dev->privsw_node, &cci_privsw_devices);
	mutex_unlock(&cci_privsw_devices_lock);
}

void cam_cci_privsw_remove(struct cci_device *cci_dev)
{
	mutex_lock(&cci_privsw_devices_lock);
	list_del(&cci_dev->privsw_node);
	mutex_unlock(&cci_privsw_devices_lock);
}

int cam_cci_privsw_init(void)
{
	return privsw_register_client(PRIVSW_CAMERA, &cci_privsw_nb);
}

void cam_cci_privsw_exit(void)
{
	privsw_unregister_client(PRIVSW_CAMERA, &cci_privsw_nb);
}
