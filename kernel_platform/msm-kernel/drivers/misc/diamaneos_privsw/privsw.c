// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Privacy switch: a slider read from one GPIO. It is reported to userspace as
 * an EV_SW switch (as gpio-keys did), and it tells kernel clients when to
 * block the built-in sensors.
 *
 * - The truth is the GPIO level, never input events: sendevent, uinput or
 *   evdev writes reach the input layer, not the line. The driver holds the
 *   line, so GPIO character-device requests for it fail with EBUSY.
 * - The blocking position is the logical GPIO value in
 *   "de.diamaneos,blocking-value" (default 1). EV_SW reports the logical value.
 * - A move into the blocking position applies at once; a move out of it once
 *   the level has been stable for the debounce time. A failed read counts as
 *   blocking. Before the first read everything is blocked.
 * - The policy (which sensors the switch blocks) is written once, early in
 *   boot, to /sys/kernel/privacy_switch/policy and sealed: later writes fail.
 *   Until it is written, every sensor is armed (fail closed).
 * - The driver cannot be unbound and the module cannot be unloaded.
 */

#define pr_fmt(fmt) "privsw: " fmt

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>

#include "privsw_core.h"

#define PRIVSW_DEFAULT_DEBOUNCE_MS	15

struct privsw_dev {
	struct device *dev;
	struct gpio_desc *gpiod;
	struct input_dev *input;
	unsigned int code;
	int blocking_value;	/* the logical GPIO value of the blocking position */
	unsigned int debounce_ms;
	int irq;
	struct delayed_work settle_work;
};

static DEFINE_MUTEX(privsw_lock);
static struct privsw_core core;			/* guarded by privsw_lock */
static struct privsw_dev *privsw;		/* the one switch, guarded by privsw_lock */
static unsigned long privsw_blocked_bits;	/* lock-free copy for privsw_blocked() */
static struct blocking_notifier_head privsw_chains[PRIVSW_NR_SENSORS];
static struct kobject *privsw_kobj;

/* Publishes the state after a change and tells the clients. */
static void privsw_publish_locked(u32 old_blocked)
{
	u32 new_blocked = privsw_core_blocked(&core);
	u32 changed = old_blocked ^ new_blocked;
	int s;

	lockdep_assert_held(&privsw_lock);

	/* Blocking: visible to privsw_blocked() before the clients act. */
	for (s = 0; s < PRIVSW_NR_SENSORS; s++)
		if (new_blocked & BIT(s))
			set_bit(s, &privsw_blocked_bits);

	for (s = 0; s < PRIVSW_NR_SENSORS; s++)
		if (changed & BIT(s))
			blocking_notifier_call_chain(&privsw_chains[s],
				(new_blocked & BIT(s)) ? PRIVSW_BLOCK : PRIVSW_UNBLOCK,
				NULL);

	/* Unblocking: only after the clients have released their hardware. */
	for (s = 0; s < PRIVSW_NR_SENSORS; s++)
		if (!(new_blocked & BIT(s)))
			clear_bit(s, &privsw_blocked_bits);

	if (changed)
		pr_info("blocked %#x -> %#x\n", old_blocked, new_blocked);
}

static void privsw_report_locked(void)
{
	lockdep_assert_held(&privsw_lock);
	if (!privsw)
		return;
	/* The logical GPIO value, as gpio-keys reported it. */
	input_report_switch(privsw->input, privsw->code,
			    core.down ? privsw->blocking_value : !privsw->blocking_value);
	input_sync(privsw->input);
}

/*
 * Reads the line and applies it, or waits for it to settle. Reading and
 * applying happen under the lock, so the last sample always wins.
 */
static void privsw_sample(struct privsw_dev *p, bool settled)
{
	int level;
	bool down;
	u32 old;

	mutex_lock(&privsw_lock);
	level = gpiod_get_value_cansleep(p->gpiod);
	down = privsw_core_level_down(level, p->blocking_value);
	if (level < 0)
		dev_warn_ratelimited(p->dev, "read failed (%d), blocking\n", level);

	if (privsw_core_apply_now(down, settled)) {
		if (down)
			cancel_delayed_work(&p->settle_work);
		old = privsw_core_blocked(&core);
		core.down = down;
		privsw_publish_locked(old);
		privsw_report_locked();
	} else {
		mod_delayed_work(system_wq, &p->settle_work,
				 msecs_to_jiffies(p->debounce_ms));
	}
	mutex_unlock(&privsw_lock);
}

static void privsw_settle(struct work_struct *work)
{
	struct privsw_dev *p = container_of(to_delayed_work(work),
					    struct privsw_dev, settle_work);

	privsw_sample(p, true);
}

static irqreturn_t privsw_irq(int irq, void *data)
{
	privsw_sample(data, false);
	return IRQ_HANDLED;
}

/* Kernel client API */

int privsw_register_client(enum privsw_sensor sensor, struct notifier_block *nb)
{
	int ret;

	if (sensor >= PRIVSW_NR_SENSORS || !nb || !nb->notifier_call)
		return -EINVAL;

	mutex_lock(&privsw_lock);
	ret = blocking_notifier_chain_register(&privsw_chains[sensor], nb);
	if (!ret) {
		core.enforced |= BIT(sensor);
		nb->notifier_call(nb,
			(privsw_core_blocked(&core) & BIT(sensor)) ?
				PRIVSW_BLOCK : PRIVSW_UNBLOCK, NULL);
	}
	mutex_unlock(&privsw_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(privsw_register_client);

void privsw_unregister_client(enum privsw_sensor sensor, struct notifier_block *nb)
{
	if (sensor >= PRIVSW_NR_SENSORS || !nb)
		return;
	blocking_notifier_chain_unregister(&privsw_chains[sensor], nb);
}
EXPORT_SYMBOL_GPL(privsw_unregister_client);

bool privsw_blocked(enum privsw_sensor sensor)
{
	if (sensor >= PRIVSW_NR_SENSORS)
		return true;
	return test_bit(sensor, &privsw_blocked_bits);
}
EXPORT_SYMBOL_GPL(privsw_blocked);

/* sysfs: /sys/kernel/privacy_switch */

static ssize_t policy_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	u32 old;
	int ret;

	mutex_lock(&privsw_lock);
	old = privsw_core_blocked(&core);
	ret = privsw_core_seal(&core, buf, count);
	if (!ret) {
		pr_info("policy %#x sealed\n", core.policy);
		privsw_publish_locked(old);
	}
	mutex_unlock(&privsw_lock);
	if (ret)
		pr_warn("policy write refused (%d)\n", ret);
	return ret ? ret : count;
}

/* What the kernel blocks this boot while the switch is down: a decimal mask. */
static ssize_t enforced_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	u32 value;

	mutex_lock(&privsw_lock);
	value = privsw_core_enforced(&core);
	mutex_unlock(&privsw_lock);
	return sysfs_emit(buf, "%u\n", value);
}

/* For debugging and tests. */
static ssize_t state_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	ssize_t len;

	mutex_lock(&privsw_lock);
	len = sysfs_emit(buf,
		"sealed=%d policy=%#x armed=%#x enforced=%#x switch=%s down=%d blocked=%#x\n",
		core.sealed, core.policy, privsw_core_armed(&core),
		privsw_core_enforced(&core), privsw ? "present" : "absent",
		core.down, privsw_core_blocked(&core));
	mutex_unlock(&privsw_lock);
	return len;
}

static struct kobj_attribute policy_attr = __ATTR_WO(policy);
static struct kobj_attribute enforced_attr = __ATTR_RO(enforced);
static struct kobj_attribute state_attr = __ATTR_RO(state);

static struct attribute *privsw_attrs[] = {
	&policy_attr.attr,
	&enforced_attr.attr,
	&state_attr.attr,
	NULL,
};

static const struct attribute_group privsw_group = {
	.attrs = privsw_attrs,
};

/* Platform driver */

static int privsw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct privsw_dev *p;
	u32 code, value;
	int ret;

	/* One switch only: its line is the single source of truth. */
	mutex_lock(&privsw_lock);
	ret = privsw ? -EBUSY : 0;
	mutex_unlock(&privsw_lock);
	if (ret)
		return dev_err_probe(dev, ret, "a privacy switch is already registered\n");

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->dev = dev;

	ret = of_property_read_u32(dev->of_node, "linux,code", &code);
	if (ret || code > SW_MAX) {
		dev_err(dev, "missing or invalid linux,code\n");
		return -EINVAL;
	}
	p->code = code;
	if (of_property_read_u32(dev->of_node, "de.diamaneos,blocking-value", &value))
		value = 1;
	if (value > 1) {
		dev_err(dev, "invalid de.diamaneos,blocking-value\n");
		return -EINVAL;
	}
	p->blocking_value = value;
	if (of_property_read_u32(dev->of_node, "debounce-interval",
				 &p->debounce_ms))
		p->debounce_ms = PRIVSW_DEFAULT_DEBOUNCE_MS;

	p->gpiod = devm_gpiod_get(dev, "switch", GPIOD_IN);
	if (IS_ERR(p->gpiod))
		return dev_err_probe(dev, PTR_ERR(p->gpiod), "no switch gpio\n");
	p->irq = gpiod_to_irq(p->gpiod);
	if (p->irq < 0)
		return dev_err_probe(dev, p->irq, "no irq for the switch gpio\n");

	INIT_DELAYED_WORK(&p->settle_work, privsw_settle);

	p->input = devm_input_allocate_device(dev);
	if (!p->input)
		return -ENOMEM;
	p->input->name = "privacy-switch";
	p->input->phys = "privacy-switch/input0";
	p->input->id.bustype = BUS_HOST;
	p->input->id.vendor = 0x0001;
	p->input->id.product = 0x0001;
	p->input->id.version = 0x0100;
	input_set_capability(p->input, EV_SW, p->code);
	ret = input_register_device(p->input);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, p);
	ret = devm_request_threaded_irq(dev, p->irq, NULL, privsw_irq,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT, "privacy-switch", p);
	if (ret)
		return dev_err_probe(dev, ret, "no switch irq\n");

	mutex_lock(&privsw_lock);
	privsw = p;
	/* Report the fail-closed starting state until the first read. */
	privsw_report_locked();
	mutex_unlock(&privsw_lock);

	device_init_wakeup(dev, of_property_read_bool(dev->of_node, "wakeup-source"));
	privsw_sample(p, false);
	return 0;
}

static int privsw_suspend(struct device *dev)
{
	struct privsw_dev *p = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(p->irq);
	return 0;
}

static int privsw_resume(struct device *dev)
{
	struct privsw_dev *p = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(p->irq);
	/* A move during suspend: read the line again. */
	privsw_sample(p, false);
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(privsw_pm_ops, privsw_suspend, privsw_resume);

static const struct of_device_id privsw_of_match[] = {
	{ .compatible = "de.diamaneos,privacy-switch" },
	{ }
};
MODULE_DEVICE_TABLE(of, privsw_of_match);

static struct platform_driver privsw_driver = {
	.probe = privsw_probe,
	.driver = {
		.name = "diamaneos-privsw",
		.of_match_table = privsw_of_match,
		.pm = pm_sleep_ptr(&privsw_pm_ops),
		/* No unbind or bind from userspace. */
		.suppress_bind_attrs = true,
	},
};

static int __init privsw_init(void)
{
	int s, ret;

	privsw_core_init(&core);
	for (s = 0; s < PRIVSW_NR_SENSORS; s++) {
		BLOCKING_INIT_NOTIFIER_HEAD(&privsw_chains[s]);
		set_bit(s, &privsw_blocked_bits);
	}

	privsw_kobj = kobject_create_and_add("privacy_switch", kernel_kobj);
	if (!privsw_kobj)
		return -ENOMEM;
	ret = sysfs_create_group(privsw_kobj, &privsw_group);
	if (ret)
		goto err_kobj;
	ret = platform_driver_register(&privsw_driver);
	if (ret)
		goto err_kobj;
	return 0;

err_kobj:
	kobject_put(privsw_kobj);
	return ret;
}
module_init(privsw_init);
/* No module_exit: the module stays loaded until reboot. */

MODULE_DESCRIPTION("DiamaneOS privacy switch");
MODULE_LICENSE("GPL");
