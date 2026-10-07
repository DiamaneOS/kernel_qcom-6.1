// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * KUnit tests for the camera floor's CCI filter (cam_cci_privsw_core.h).
 * Not part of camera.ko; run on demand (KERNEL-PRIVACY.md).
 */

#include <kunit/test.h>

#include "cam_cci_privsw_core.h"

#define V_PASS		CCI_PRIVSW_PASS
#define V_CHANGED	CCI_PRIVSW_CHANGED
#define V_DENY		CCI_PRIVSW_DENY

static u32 entry(struct kunit *test, u16 sid, struct cci_privsw_state *st,
		 bool blocked, u16 reg, u8 n, u32 data, int want)
{
	const struct cci_privsw_sensor *s = cci_privsw_find(sid);

	KUNIT_ASSERT_NOT_NULL(test, s);
	KUNIT_EXPECT_EQ(test, (int)cci_privsw_entry(s, st, blocked, reg, false, n, &data), want);
	return data;
}

static void privsw_cci_knows_the_fp6_sensors(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, cci_privsw_find(0x1a)->name, "imx896");
	KUNIT_EXPECT_STREQ(test, cci_privsw_find(0x36)->name, "ov13b10");
	KUNIT_EXPECT_STREQ(test, cci_privsw_find(0x3d)->name, "s5kkd1sp");
	KUNIT_EXPECT_NULL(test, cci_privsw_find(0x0c));
	KUNIT_EXPECT_NULL(test, cci_privsw_find(0x00));
}

static void privsw_cci_ccs_pattern_forced(struct kunit *test)
{
	struct cci_privsw_state st = {};

	/* CamX turns its own pattern off: the sensor gets solid colour. */
	KUNIT_EXPECT_EQ(test, entry(test, 0x1a, &st, true, 0x0600, 2, 0x0000, V_CHANGED), 0x0001u);
	/* An 8-bit write to the mode byte alone. */
	KUNIT_EXPECT_EQ(test, entry(test, 0x1a, &st, true, 0x0601, 1, 0x02, V_CHANGED), 0x01u);
	/* Colour data: zero, so black. */
	KUNIT_EXPECT_EQ(test, entry(test, 0x1a, &st, true, 0x0602, 2, 0x03ff, V_CHANGED), 0x0000u);
	KUNIT_EXPECT_EQ(test, entry(test, 0x1a, &st, true, 0x0608, 2, 0x0123, V_CHANGED), 0x0000u);
	/* Already the forced value: unchanged. */
	entry(test, 0x1a, &st, true, 0x0604, 2, 0x0000, V_PASS);
	/* A 32-bit write across the block edge changes only the block's bytes. */
	KUNIT_EXPECT_EQ(test, entry(test, 0x1a, &st, true, 0x0608, 4, 0x11223344, V_CHANGED),
			0x00003344u);
	/* CamX's values are remembered. */
	KUNIT_EXPECT_EQ(test, st.shadow[1], 0x02);
	KUNIT_EXPECT_EQ(test, st.shadow[2], 0x03);
	KUNIT_EXPECT_EQ(test, st.shadow[3], 0xff);
}

static void privsw_cci_neighbours_untouched(struct kunit *test)
{
	struct cci_privsw_state st = {};

	entry(test, 0x1a, &st, true, 0x05fe, 2, 0xabcd, V_PASS);
	entry(test, 0x1a, &st, true, 0x060a, 2, 0xabcd, V_PASS);
	entry(test, 0x1a, &st, true, 0x0100, 1, 0x01, V_PASS);	/* stream on */
	entry(test, 0x1a, &st, true, 0x0103, 1, 0x01, V_PASS);	/* reset: pattern follows */
}

static void privsw_cci_unblocked_only_remembers(struct kunit *test)
{
	struct cci_privsw_state st = {};

	entry(test, 0x1a, &st, false, 0x0600, 2, 0x0002, V_PASS);
	entry(test, 0x1a, &st, false, 0x0107, 1, 0x20, V_PASS);
	KUNIT_EXPECT_EQ(test, st.shadow[1], 0x02);
}

static void privsw_cci_readdressing_refused(struct kunit *test)
{
	struct cci_privsw_state st = {};

	entry(test, 0x1a, &st, true, 0x0107, 1, 0x20, V_DENY);
	entry(test, 0x1a, &st, true, 0x0109, 1, 0x01, V_DENY);
	/* A 16-bit write that ends on a re-addressing register. */
	entry(test, 0x3d, &st, true, 0x0106, 2, 0x0020, V_DENY);
}

static void privsw_cci_ov_enable_bit(struct kunit *test)
{
	struct cci_privsw_state st = {};

	/* CamX's bits stay; bit 7 (pattern on) is forced. */
	KUNIT_EXPECT_EQ(test, entry(test, 0x36, &st, true, 0x5080, 1, 0x0c, V_CHANGED), 0x8cu);
	KUNIT_EXPECT_EQ(test, st.shadow[0], 0x0c);
	entry(test, 0x36, &st, true, 0x5081, 1, 0x00, V_PASS);
	entry(test, 0x36, &st, false, 0x5080, 1, 0x00, V_PASS);
	KUNIT_EXPECT_EQ(test, cci_privsw_forced(cci_privsw_find(0x36), &st, 0), 0x80);
}

static void privsw_cci_samsung_window(struct kunit *test)
{
	struct cci_privsw_state st = {};

	/* Page 0x4000 (standard registers), pointer 0x0600, then data. */
	entry(test, 0x3d, &st, true, 0x6028, 2, 0x4000, V_PASS);
	entry(test, 0x3d, &st, true, 0x602a, 2, 0x0600, V_PASS);
	KUNIT_EXPECT_EQ(test, entry(test, 0x3d, &st, true, 0x6f12, 2, 0x0000, V_CHANGED), 0x0001u);
	/* The window moved to 0x0602: colour data. */
	KUNIT_EXPECT_EQ(test, entry(test, 0x3d, &st, true, 0x6f12, 2, 0x1234, V_CHANGED), 0x0000u);
	KUNIT_EXPECT_EQ(test, st.ss_ptr, 0x0604);
	/* Another page (sensor firmware): not filtered. */
	entry(test, 0x3d, &st, true, 0x6028, 2, 0x2000, V_PASS);
	entry(test, 0x3d, &st, true, 0x602a, 2, 0x0600, V_PASS);
	entry(test, 0x3d, &st, true, 0x6f12, 2, 0x1234, V_PASS);
}

static void privsw_cci_samsung_window_deny(struct kunit *test)
{
	struct cci_privsw_state st = {};

	entry(test, 0x3d, &st, true, 0x6028, 2, 0x4000, V_PASS);
	entry(test, 0x3d, &st, true, 0x602a, 2, 0x0106, V_PASS);
	entry(test, 0x3d, &st, true, 0x6f12, 2, 0x0020, V_DENY);
}

static void privsw_cci_samsung_burst_dataport(struct kunit *test)
{
	const struct cci_privsw_sensor *s = cci_privsw_find(0x3d);
	struct cci_privsw_state st = { .ss_page = 0x4000, .ss_ptr = 0x0600 };
	u32 d = 0x0000;

	/* A burst into 0x6F12: every word lands in the window. */
	KUNIT_EXPECT_EQ(test, (int)cci_privsw_entry(s, &st, true, 0x6f12, true, 2, &d), V_CHANGED);
	KUNIT_EXPECT_EQ(test, d, 0x0001u);
	d = 0x5555;
	KUNIT_EXPECT_EQ(test, (int)cci_privsw_entry(s, &st, true, 0x6f14, true, 2, &d), V_CHANGED);
	KUNIT_EXPECT_EQ(test, d, 0x0000u);
	KUNIT_EXPECT_EQ(test, st.ss_ptr, 0x0604);
	/* The IMX896 has no window: 0x6F12 is an ordinary register there. */
	entry(test, 0x1a, &st, true, 0x6f12, 2, 0x0000, V_PASS);
}

static struct kunit_case privsw_cci_cases[] = {
	KUNIT_CASE(privsw_cci_knows_the_fp6_sensors),
	KUNIT_CASE(privsw_cci_ccs_pattern_forced),
	KUNIT_CASE(privsw_cci_neighbours_untouched),
	KUNIT_CASE(privsw_cci_unblocked_only_remembers),
	KUNIT_CASE(privsw_cci_readdressing_refused),
	KUNIT_CASE(privsw_cci_ov_enable_bit),
	KUNIT_CASE(privsw_cci_samsung_window),
	KUNIT_CASE(privsw_cci_samsung_window_deny),
	KUNIT_CASE(privsw_cci_samsung_burst_dataport),
	{}
};

static struct kunit_suite privsw_cci_suite = {
	.name = "cam-cci-privsw",
	.test_cases = privsw_cci_cases,
};
kunit_test_suite(privsw_cci_suite);

MODULE_DESCRIPTION("KUnit tests for the camera floor's CCI filter");
MODULE_LICENSE("GPL");
