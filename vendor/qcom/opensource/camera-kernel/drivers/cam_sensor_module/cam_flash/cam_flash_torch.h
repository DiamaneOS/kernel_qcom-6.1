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
 * Current for one torch channel in mA: the HAL's request scaled by the level
 * (a quarter, a half, as asked, three times). The torch switch shares the sum
 * of its channels evenly between its LEDs, and the HAL may drive only one
 * channel, so scaling keeps each LED at the level's share whichever it drives.
 * Never above max_ma.
 */
static inline u32 cam_flash_torch_current(u32 level, u32 hal_ma, u32 max_ma)
{
	static const u32 level_quarters[CAM_FLASH_TORCH_LEVEL_MAX] = { 1, 2, 4, 12 };
	u64 ma = hal_ma;

	if (cam_flash_torch_level_valid(level))
		ma = (u64)hal_ma * level_quarters[level - 1] / 4;
	return min_t(u64, ma, max_ma);
}

#endif /* _CAM_FLASH_TORCH_H_ */
