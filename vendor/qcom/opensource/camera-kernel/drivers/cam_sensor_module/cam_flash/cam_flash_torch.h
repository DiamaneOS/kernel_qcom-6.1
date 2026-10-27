/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Flashlight strength: the torch current for each level. Free of driver state
 * so KUnit can test it (cam_flash_torch_test.c).
 */

#ifndef _CAM_FLASH_TORCH_H_
#define _CAM_FLASH_TORCH_H_

#include <linux/minmax.h>
#include <linux/types.h>

#define CAM_FLASH_TORCH_LEVEL_MIN	1
#define CAM_FLASH_TORCH_LEVEL_MAX	4
#define CAM_FLASH_TORCH_LEVEL_DEFAULT	3

static inline bool cam_flash_torch_level_valid(u32 level)
{
	return level >= CAM_FLASH_TORCH_LEVEL_MIN &&
		level <= CAM_FLASH_TORCH_LEVEL_MAX;
}

/*
 * Current for one torch LED in mA. The default level keeps the HAL's request,
 * the others replace it for the LEDs the HAL turned on. Never above max_ma.
 */
static inline u32 cam_flash_torch_current(u32 level, u32 hal_ma, u32 max_ma)
{
	static const u32 level_ma[CAM_FLASH_TORCH_LEVEL_MAX] = { 15, 30, 0, 150 };
	u32 ma = hal_ma;

	if (hal_ma && level != CAM_FLASH_TORCH_LEVEL_DEFAULT &&
	    cam_flash_torch_level_valid(level))
		ma = level_ma[level - 1];
	return min(ma, max_ma);
}

#endif /* _CAM_FLASH_TORCH_H_ */
