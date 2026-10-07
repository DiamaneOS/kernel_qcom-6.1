// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * KUnit tests for the privacy switch's state machine (privsw_core.h).
 */

#include <kunit/test.h>

#include "privsw_core.h"

#define MIC	BIT(PRIVSW_MIC)
#define CAMERA	BIT(PRIVSW_CAMERA)

static int seal(struct privsw_core *c, const char *s)
{
	return privsw_core_seal(c, s, strlen(s));
}

static void privsw_starts_closed(struct kunit *test)
{
	struct privsw_core c;

	privsw_core_init(&c);
	KUNIT_EXPECT_FALSE(test, c.sealed);
	KUNIT_EXPECT_TRUE(test, c.down);
	KUNIT_EXPECT_EQ(test, privsw_core_armed(&c), (u32)PRIVSW_ALL);
	KUNIT_EXPECT_EQ(test, privsw_core_blocked(&c), (u32)PRIVSW_ALL);
	/* Nothing enforced until a client registers. */
	KUNIT_EXPECT_EQ(test, privsw_core_enforced(&c), 0u);
}

static void privsw_seals_once(struct kunit *test)
{
	struct privsw_core c;

	privsw_core_init(&c);
	KUNIT_EXPECT_EQ(test, seal(&c, "1\n"), 0);
	KUNIT_EXPECT_TRUE(test, c.sealed);
	KUNIT_EXPECT_EQ(test, privsw_core_armed(&c), (u32)MIC);
	/* No disarming, no raising, no rewriting the same value. */
	KUNIT_EXPECT_EQ(test, seal(&c, "0"), -EPERM);
	KUNIT_EXPECT_EQ(test, seal(&c, "3"), -EPERM);
	KUNIT_EXPECT_EQ(test, seal(&c, "1"), -EPERM);
	KUNIT_EXPECT_EQ(test, privsw_core_armed(&c), (u32)MIC);
}

static void privsw_disarmed_policy(struct kunit *test)
{
	struct privsw_core c;

	privsw_core_init(&c);
	KUNIT_EXPECT_EQ(test, seal(&c, "0"), 0);
	KUNIT_EXPECT_EQ(test, privsw_core_armed(&c), 0u);
	c.down = true;
	KUNIT_EXPECT_EQ(test, privsw_core_blocked(&c), 0u);
	KUNIT_EXPECT_EQ(test, seal(&c, "3"), -EPERM);
}

static void privsw_bad_writes_do_not_seal(struct kunit *test)
{
	static const char * const bad[] = {
		"", "4", "7", "-1", "0x1", "1 ", " 1", "mic", "99999999999",
		"1\n2", "4294967297",
	};
	struct privsw_core c;
	int i;

	privsw_core_init(&c);
	for (i = 0; i < ARRAY_SIZE(bad); i++) {
		KUNIT_EXPECT_EQ_MSG(test, seal(&c, bad[i]), -EINVAL, "write \"%s\"", bad[i]);
		KUNIT_EXPECT_FALSE(test, c.sealed);
	}
	/* Still fail closed, and a good write still works afterwards. */
	KUNIT_EXPECT_EQ(test, privsw_core_armed(&c), (u32)PRIVSW_ALL);
	KUNIT_EXPECT_EQ(test, seal(&c, "3"), 0);
	KUNIT_EXPECT_EQ(test, privsw_core_armed(&c), (u32)(MIC | CAMERA));
}

static void privsw_blocks_only_when_down(struct kunit *test)
{
	struct privsw_core c;

	privsw_core_init(&c);
	KUNIT_ASSERT_EQ(test, seal(&c, "1"), 0);
	c.down = false;
	KUNIT_EXPECT_EQ(test, privsw_core_blocked(&c), 0u);
	c.down = true;
	KUNIT_EXPECT_EQ(test, privsw_core_blocked(&c), (u32)MIC);
}

static void privsw_enforced_needs_a_client(struct kunit *test)
{
	struct privsw_core c;

	privsw_core_init(&c);
	KUNIT_ASSERT_EQ(test, seal(&c, "3"), 0);
	c.enforced |= MIC;
	/* The camera is armed but has no client yet: not enforced. */
	KUNIT_EXPECT_EQ(test, privsw_core_enforced(&c), (u32)MIC);
	c.enforced |= CAMERA;
	KUNIT_EXPECT_EQ(test, privsw_core_enforced(&c), (u32)(MIC | CAMERA));
}

static void privsw_read_errors_block(struct kunit *test)
{
	/* Blocking at logical 1 (the default). */
	KUNIT_EXPECT_TRUE(test, privsw_core_level_down(1, 1));
	KUNIT_EXPECT_FALSE(test, privsw_core_level_down(0, 1));
	/* Blocking at logical 0, as on the FP6. */
	KUNIT_EXPECT_TRUE(test, privsw_core_level_down(0, 0));
	KUNIT_EXPECT_FALSE(test, privsw_core_level_down(1, 0));
	/* A failed read blocks either way. */
	KUNIT_EXPECT_TRUE(test, privsw_core_level_down(-EIO, 0));
	KUNIT_EXPECT_TRUE(test, privsw_core_level_down(-EIO, 1));
	KUNIT_EXPECT_TRUE(test, privsw_core_level_down(-EPROBE_DEFER, 0));
}

static void privsw_debounce_favours_privacy(struct kunit *test)
{
	/* Down applies at once; up only once settled. */
	KUNIT_EXPECT_TRUE(test, privsw_core_apply_now(true, false));
	KUNIT_EXPECT_TRUE(test, privsw_core_apply_now(true, true));
	KUNIT_EXPECT_FALSE(test, privsw_core_apply_now(false, false));
	KUNIT_EXPECT_TRUE(test, privsw_core_apply_now(false, true));
}

static struct kunit_case privsw_test_cases[] = {
	KUNIT_CASE(privsw_starts_closed),
	KUNIT_CASE(privsw_seals_once),
	KUNIT_CASE(privsw_disarmed_policy),
	KUNIT_CASE(privsw_bad_writes_do_not_seal),
	KUNIT_CASE(privsw_blocks_only_when_down),
	KUNIT_CASE(privsw_enforced_needs_a_client),
	KUNIT_CASE(privsw_read_errors_block),
	KUNIT_CASE(privsw_debounce_favours_privacy),
	{}
};

static struct kunit_suite privsw_test_suite = {
	.name = "diamaneos-privsw",
	.test_cases = privsw_test_cases,
};
kunit_test_suite(privsw_test_suite);

MODULE_DESCRIPTION("KUnit tests for the DiamaneOS privacy switch");
MODULE_LICENSE("GPL");
