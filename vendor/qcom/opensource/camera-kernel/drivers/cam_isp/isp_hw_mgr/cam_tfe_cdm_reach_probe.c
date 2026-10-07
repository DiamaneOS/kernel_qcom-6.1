// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * CDM reach probe, test builds only (CONFIG_SPECTRA_CDM_REACH_PROBE; the
 * release configuration never sets it and an image check refuses a
 * camera.ko that contains it).
 *
 * Question (KERNEL-PRIVACY.md, Q1): the camera HAL's command buffers run on
 * the TFE's real-time CDM unchecked. The CDM addresses registers relative to
 * the TFE wrapper (0xac62000). Can a CHANGE_BASE reach a block outside it,
 * the CCI (0xac15000/0xac16000) in particular?
 *
 * Use (root, rear camera previewing so the TFE, CDM and CCI are clocked):
 *   echo <base>   > /sys/module/camera/parameters/cdm_reach_probe_base
 *   echo <offset> > /sys/module/camera/parameters/cdm_reach_probe_offset
 *   echo <phys>   > /sys/module/camera/parameters/cdm_reach_probe_readback
 *   echo 1        > /sys/module/camera/parameters/cdm_reach_probe_arm
 * At the next TFE configuration the kernel appends one command buffer from
 * its own memory: CHANGE_BASE <base>, REG_RANDOM {offset, value},
 * CHANGE_BASE 0. It logs the register at <phys> (0: none; only for clocked
 * blocks) before, and 100 ms after. An AHB error is reported by the CDM
 * driver's own error dump (last_ahb_err_addr).
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#include "cam_cdm_util.h"
#include "cam_debug_util.h"
#include "cam_mem_mgr_api.h"
#include "cam_tfe_cdm_reach_probe.h"

static uint cdm_reach_probe_base;
module_param(cdm_reach_probe_base, uint, 0600);
MODULE_PARM_DESC(cdm_reach_probe_base, "test only: CDM base (24-bit, wrapper-relative)");
static uint cdm_reach_probe_offset;
module_param(cdm_reach_probe_offset, uint, 0600);
MODULE_PARM_DESC(cdm_reach_probe_offset, "test only: register offset from the base");
static uint cdm_reach_probe_value = 0xA5A5A5A5;
module_param(cdm_reach_probe_value, uint, 0600);
MODULE_PARM_DESC(cdm_reach_probe_value, "test only: value written");
static ulong cdm_reach_probe_readback;
module_param(cdm_reach_probe_readback, ulong, 0600);
MODULE_PARM_DESC(cdm_reach_probe_readback, "test only: physical address read before and after (0: none)");
static bool cdm_reach_probe_arm;
module_param(cdm_reach_probe_arm, bool, 0600);
MODULE_PARM_DESC(cdm_reach_probe_arm, "test only: 1 runs the probe at the next TFE configuration");

static struct cam_mem_mgr_memory_desc probe_buf;
static bool probe_buf_ready;
static phys_addr_t probe_readback;

static void cam_tfe_cdm_reach_after(struct work_struct *work)
{
	void __iomem *reg = ioremap(probe_readback, 4);

	if (!reg)
		return;
	CAM_INFO(CAM_ISP, "CDM reach probe: after: 0x%08x at %pa", readl(reg), &probe_readback);
	iounmap(reg);
}
static DECLARE_DELAYED_WORK(probe_after_work, cam_tfe_cdm_reach_after);

uint32_t cam_tfe_cdm_reach_probe(struct cam_cdm_bl_request *cdm_cmd,
	uint32_t count, uint32_t max, int32_t smmu_hdl)
{
	struct cam_mem_mgr_request_desc req = {
		.size = PAGE_SIZE,
		.flags = CAM_MEM_FLAG_HW_READ_WRITE | CAM_MEM_FLAG_HW_SHARED_ACCESS,
		.smmu_hdl = smmu_hdl,
	};
	uint32_t *cmd;
	int rc;

	if (!READ_ONCE(cdm_reach_probe_arm) || count >= max)
		return count;
	WRITE_ONCE(cdm_reach_probe_arm, false);

	if (!probe_buf_ready) {
		rc = cam_mem_mgr_request_mem(&req, &probe_buf);
		if (rc) {
			CAM_ERR(CAM_ISP, "CDM reach probe: no buffer: %d", rc);
			return count;
		}
		probe_buf_ready = true;
	}

	cmd = (uint32_t *)probe_buf.kva;
	cmd[0] = (CAM_CDM_CMD_CHANGE_BASE << CAM_CDM_COMMAND_OFFSET) |
		 (cdm_reach_probe_base & 0xFFFFFF);
	cmd[1] = (CAM_CDM_CMD_REG_RANDOM << CAM_CDM_COMMAND_OFFSET) | 1;
	cmd[2] = cdm_reach_probe_offset & 0xFFFFFF;
	cmd[3] = cdm_reach_probe_value;
	cmd[4] = CAM_CDM_CMD_CHANGE_BASE << CAM_CDM_COMMAND_OFFSET;
	wmb();

	probe_readback = cdm_reach_probe_readback;
	if (probe_readback) {
		void __iomem *reg = ioremap(probe_readback, 4);

		if (reg) {
			CAM_INFO(CAM_ISP, "CDM reach probe: before: 0x%08x at %pa",
				readl(reg), &probe_readback);
			iounmap(reg);
		}
		schedule_delayed_work(&probe_after_work, msecs_to_jiffies(100));
	}

	cdm_cmd->cmd[count].bl_addr.mem_handle = probe_buf.mem_handle;
	cdm_cmd->cmd[count].offset = 0;
	cdm_cmd->cmd[count].len = 5 * sizeof(uint32_t);
	cdm_cmd->cmd[count].arbitrate = false;
	CAM_INFO(CAM_ISP, "CDM reach probe: base 0x%06x offset 0x%06x value 0x%08x queued",
		cdm_reach_probe_base & 0xFFFFFF, cdm_reach_probe_offset & 0xFFFFFF,
		cdm_reach_probe_value);
	return count + 1;
}
