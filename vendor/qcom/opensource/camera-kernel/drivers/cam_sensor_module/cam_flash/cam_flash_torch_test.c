// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * KUnit tests for the flashlight strength levels (cam_flash_torch.h).
 * Not part of camera.ko. Run on demand.
 */

#include <kunit/test.h>

#include "cam_flash_torch.h"

static void torch_levels_scale_the_hal_request(struct kunit *test)
{
	/* The FP6 HAL asks 100 mA on one channel, the switch shares it between two LEDs. */
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(1, 100, 500), 25U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(2, 100, 500), 50U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(3, 100, 500), 100U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(4, 100, 500), 300U);
	/* Both channels driven: each keeps its own share. */
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(1, 50, 500), 12U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(4, 50, 500), 150U);
}

static void torch_default_keeps_the_hal_request(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(3, 75, 500), 75U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(3, 600, 500), 500U);
}

static void torch_stays_within_the_led_maximum(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(4, 50, 100), 100U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(4, U32_MAX, 500), 500U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(1, 50, 0), 0U);
}

static void torch_led_off_stays_off(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(4, 0, 500), 0U);
}

static void torch_invalid_levels(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, cam_flash_torch_level_valid(0));
	KUNIT_EXPECT_TRUE(test, cam_flash_torch_level_valid(1));
	KUNIT_EXPECT_TRUE(test, cam_flash_torch_level_valid(4));
	KUNIT_EXPECT_FALSE(test, cam_flash_torch_level_valid(5));
	KUNIT_EXPECT_FALSE(test, cam_flash_torch_level_valid(U32_MAX));
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(0, 50, 500), 50U);
	KUNIT_EXPECT_EQ(test, cam_flash_torch_current(5, 50, 500), 50U);
}

static struct kunit_case cam_flash_torch_cases[] = {
	KUNIT_CASE(torch_levels_scale_the_hal_request),
	KUNIT_CASE(torch_default_keeps_the_hal_request),
	KUNIT_CASE(torch_stays_within_the_led_maximum),
	KUNIT_CASE(torch_led_off_stays_off),
	KUNIT_CASE(torch_invalid_levels),
	{}
};

static struct kunit_suite cam_flash_torch_suite = {
	.name = "cam-flash-torch",
	.test_cases = cam_flash_torch_cases,
};
kunit_test_suite(cam_flash_torch_suite);

MODULE_DESCRIPTION("KUnit tests for the flashlight strength levels");
MODULE_LICENSE("GPL");
