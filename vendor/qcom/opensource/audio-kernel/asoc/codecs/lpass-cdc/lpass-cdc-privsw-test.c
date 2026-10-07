// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * KUnit tests for the LPASS codec's mic floor filter (lpass-cdc-privsw.h).
 * Built only with CONFIG_LPASS_CDC_PRIVSW_KUNIT_TEST; needs no hardware.
 */

#include <kunit/test.h>

#include "lpass-cdc-privsw.h"

#define TX	LPASS_CDC_PRIVSW_TX_MACRO
#define VA	LPASS_CDC_PRIVSW_VA_MACRO
#define RX	1	/* RX_MACRO */
#define WSA	2	/* WSA_MACRO */

static void privsw_finds_every_decimator(struct kunit *test)
{
	unsigned int n;

	for (n = 0; n < 8; n++)
		KUNIT_EXPECT_TRUE(test, lpass_cdc_privsw_is_dec_ctl(TX,
			LPASS_CDC_TX0_TX_PATH_CTL - TX_START_OFFSET + n * 0x80));
	for (n = 0; n < 4; n++)
		KUNIT_EXPECT_TRUE(test, lpass_cdc_privsw_is_dec_ctl(VA,
			LPASS_CDC_VA_TX0_TX_PATH_CTL - VA_START_OFFSET + n * 0x80));
	KUNIT_EXPECT_TRUE(test, lpass_cdc_privsw_is_dec_ctl(TX,
		LPASS_CDC_TX7_TX_PATH_CTL - TX_START_OFFSET));
	KUNIT_EXPECT_TRUE(test, lpass_cdc_privsw_is_dec_ctl(VA,
		LPASS_CDC_VA_TX3_TX_PATH_CTL - VA_START_OFFSET));
}

static void privsw_leaves_other_registers(struct kunit *test)
{
	/* Neighbours of a path control register. */
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(TX,
		LPASS_CDC_TX0_TX_PATH_CFG0 - TX_START_OFFSET));
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(TX,
		LPASS_CDC_TX0_TX_VOL_CTL - TX_START_OFFSET));
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(TX, 0x03fc));
	/* Past the last decimator. */
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(TX, 0x0800));
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(VA, 0x0600));
	/* Other macros, even at the same offset. */
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(RX, 0x0400));
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(WSA, 0x0400));
	KUNIT_EXPECT_FALSE(test, lpass_cdc_privsw_is_dec_ctl(7, 0x0400));
}

static void privsw_forces_the_mute_bit(struct kunit *test)
{
	u16 dec0 = LPASS_CDC_TX0_TX_PATH_CTL - TX_START_OFFSET;
	u16 va1 = LPASS_CDC_VA_TX1_TX_PATH_CTL - VA_START_OFFSET;

	/* The unmute work clears bit 4 of a running path (0x34 -> 0x24). */
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(true, TX, dec0, 0x24), 0x34);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(true, VA, va1, 0x24), 0x34);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(true, TX, dec0, 0x00), 0x10);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(true, TX, dec0, 0x34), 0x34);
	/* Not muted: values pass unchanged. */
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(false, TX, dec0, 0x24), 0x24);
	/* Muted, but another register: unchanged. */
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(true, TX, dec0 + 4, 0x24), 0x24);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_filter(true, RX, dec0, 0x24), 0x24);
}

static void privsw_rewrite_list_matches_the_filter(struct kunit *test)
{
	unsigned int n, reg;

	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_dec_ctl_reg(0), LPASS_CDC_TX0_TX_PATH_CTL);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_dec_ctl_reg(7), LPASS_CDC_TX7_TX_PATH_CTL);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_dec_ctl_reg(8), LPASS_CDC_VA_TX0_TX_PATH_CTL);
	KUNIT_EXPECT_EQ(test, lpass_cdc_privsw_dec_ctl_reg(11), LPASS_CDC_VA_TX3_TX_PATH_CTL);
	for (n = 0; n < LPASS_CDC_PRIVSW_ALL_DECS; n++) {
		reg = lpass_cdc_privsw_dec_ctl_reg(n);
		if (n < LPASS_CDC_PRIVSW_TX_DECS)
			KUNIT_EXPECT_TRUE(test, lpass_cdc_privsw_is_dec_ctl(TX,
				reg - TX_START_OFFSET));
		else
			KUNIT_EXPECT_TRUE(test, lpass_cdc_privsw_is_dec_ctl(VA,
				reg - VA_START_OFFSET));
	}
}

static struct kunit_case lpass_cdc_privsw_cases[] = {
	KUNIT_CASE(privsw_finds_every_decimator),
	KUNIT_CASE(privsw_leaves_other_registers),
	KUNIT_CASE(privsw_forces_the_mute_bit),
	KUNIT_CASE(privsw_rewrite_list_matches_the_filter),
	{}
};

static struct kunit_suite lpass_cdc_privsw_suite = {
	.name = "lpass-cdc-privsw",
	.test_cases = lpass_cdc_privsw_cases,
};
kunit_test_suite(lpass_cdc_privsw_suite);

MODULE_DESCRIPTION("KUnit tests for the LPASS codec mic floor");
MODULE_LICENSE("GPL");
