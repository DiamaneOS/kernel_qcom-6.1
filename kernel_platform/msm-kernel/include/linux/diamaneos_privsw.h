/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Privacy switch: the in-kernel side of a hardware slider that blocks the
 * built-in sensors. Drivers that own a sensor's data path (the audio codec
 * drivers for the microphones, later the camera driver) register as clients
 * and hold their hardware in a blocked state while told to.
 *
 * The policy (which sensors the switch blocks) is written once at boot and
 * sealed until reboot. Until it is written, every sensor counts as armed.
 */

#ifndef _LINUX_DIAMANEOS_PRIVSW_H
#define _LINUX_DIAMANEOS_PRIVSW_H

#include <linux/notifier.h>
#include <linux/types.h>

enum privsw_sensor {
	PRIVSW_MIC = 0,
	PRIVSW_CAMERA = 1,
	PRIVSW_NR_SENSORS,
};

/* Notifier actions, sent to the clients of one sensor. */
#define PRIVSW_UNBLOCK	0
#define PRIVSW_BLOCK	1

/*
 * Registers @nb for @sensor and calls it once, before returning, with the
 * current state. Later calls come on every change. All calls are serialised,
 * may sleep and must not call back into this API. A registration also marks
 * the sensor as enforced in the kernel, for the rest of the boot.
 */
int privsw_register_client(enum privsw_sensor sensor, struct notifier_block *nb);
void privsw_unregister_client(enum privsw_sensor sensor, struct notifier_block *nb);

/* Whether @sensor is blocked now. Lock-free; fails closed. */
bool privsw_blocked(enum privsw_sensor sensor);

#endif /* _LINUX_DIAMANEOS_PRIVSW_H */
