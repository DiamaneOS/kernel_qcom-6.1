/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Camera floor, CCI side: which image sensors are protected, which of their
 * registers hold the test pattern, and what a write to them becomes while the
 * privacy switch blocks the cameras. Free of driver state so KUnit can test
 * it (cam_cci_privsw_test.c).
 *
 * Model: a CCI write is a run of bytes, each landing on one sensor register.
 * Writes are filtered byte by byte:
 * - test-pattern registers: CamX's value is remembered (to restore it when
 *   the switch moves back); while blocked the byte that reaches the sensor is
 *   the forced pattern;
 * - re-addressing registers (CCI address control): refused while blocked, so
 *   a sensor cannot leave the address the filter knows;
 * - Samsung's indirect window (0x6028 page, 0x602A pointer, 0x6F12 data)
 *   is followed, so writes through it into page 0x4000, the standard
 *   register space, are filtered the same way.
 */

#ifndef _CAM_CCI_PRIVSW_CORE_H_
#define _CAM_CCI_PRIVSW_CORE_H_

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#define CCI_PRIVSW_PATTERN_MAX	10
#define CCI_PRIVSW_DENY_MAX	2

/* Samsung indirect access window. */
#define CCI_PRIVSW_SS_PAGE	0x6028
#define CCI_PRIVSW_SS_PTR	0x602A
#define CCI_PRIVSW_SS_DATA	0x6F12
#define CCI_PRIVSW_SS_STD_PAGE	0x4000

struct cci_privsw_sensor {
	const char *name;
	u16 sid;			/* 7-bit CCI slave id */
	u16 pattern_reg;		/* first register of the test-pattern block */
	u8 pattern_len;
	u8 pattern_set[CCI_PRIVSW_PATTERN_MAX];	/* bits forced on while blocked */
	u8 pattern_keep[CCI_PRIVSW_PATTERN_MAX];	/* bits of CamX's value kept */
	u16 deny[CCI_PRIVSW_DENY_MAX];	/* refused while blocked */
	u8 ndeny;
	bool samsung_indirect;
};


/* Per sensor: CamX's own pattern bytes and Samsung's window position. */
struct cci_privsw_state {
	u8 shadow[CCI_PRIVSW_PATTERN_MAX];
	u16 ss_page;
	u16 ss_ptr;
};

enum cci_privsw_verdict {
	CCI_PRIVSW_PASS = 0,	/* unchanged */
	CCI_PRIVSW_CHANGED = 1,	/* some byte replaced by the pattern */
	CCI_PRIVSW_DENY = 2,	/* refused: drop the whole write */
};

/*
 * The FP6's image sensors (fp6.dtb and the CamX logs: slave 0x34, 0x6c, 0x7a).
 * - IMX896 and S5KKD1SP: CCS TEST_PATTERN_MODE 0x0600 = 0x0001 (solid
 *   colour) with the four colour values 0x0602-0x0609 = 0: black.
 * - OV13B10: 0x5080 bit 7 enables the test pattern. Only the enable bit is
 *   forced: with it set the sensor sends a synthetic pattern instead of the
 *   scene, whatever the other bits select (mainline names only colour-bar
 *   types; a black variant is to be confirmed from the CamX sensor module).
 * - 0x0107 and 0x0109 are the CCS CCI address control registers.
 */
static inline const struct cci_privsw_sensor *cci_privsw_find(u16 sid)
{
	static const struct cci_privsw_sensor sensors[] = {
		{
			.name = "imx896", .sid = 0x1a,
			.pattern_reg = 0x0600, .pattern_len = 10,
			.pattern_set = { 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0 },
			.deny = { 0x0107, 0x0109 }, .ndeny = 2,
		},
		{
			.name = "ov13b10", .sid = 0x36,
			.pattern_reg = 0x5080, .pattern_len = 1,
			.pattern_set = { 0x80 }, .pattern_keep = { 0x7f },
		},
		{
			.name = "s5kkd1sp", .sid = 0x3d,
			.pattern_reg = 0x0600, .pattern_len = 10,
			.pattern_set = { 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0 },
			.deny = { 0x0107, 0x0109 }, .ndeny = 2,
			.samsung_indirect = true,
		},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(sensors); i++)
		if (sensors[i].sid == sid)
			return &sensors[i];
	return NULL;
}

static inline u8 cci_privsw_forced(const struct cci_privsw_sensor *s,
				   const struct cci_privsw_state *st, int i)
{
	return (st->shadow[i] & s->pattern_keep[i]) | s->pattern_set[i];
}

/* One byte at a register of the sensor's own register space. */
static inline enum cci_privsw_verdict cci_privsw_reg_byte(
	const struct cci_privsw_sensor *s, struct cci_privsw_state *st,
	bool blocked, u16 reg, u8 *val)
{
	int i;

	for (i = 0; i < s->ndeny; i++)
		if (reg == s->deny[i])
			return blocked ? CCI_PRIVSW_DENY : CCI_PRIVSW_PASS;
	if (reg >= s->pattern_reg && reg < s->pattern_reg + s->pattern_len) {
		u8 forced;

		i = reg - s->pattern_reg;
		st->shadow[i] = *val;
		if (!blocked)
			return CCI_PRIVSW_PASS;
		forced = cci_privsw_forced(s, st, i);
		if (forced == *val)
			return CCI_PRIVSW_PASS;
		*val = forced;
		return CCI_PRIVSW_CHANGED;
	}
	return CCI_PRIVSW_PASS;
}

static inline void cci_privsw_set_byte(u16 *word, bool high, u8 val)
{
	*word = high ? (*word & 0x00ff) | (val << 8) : (*word & 0xff00) | val;
}

/*
 * One byte as the I2C transaction addresses it. @dataport: the byte goes to
 * Samsung's data port (a sequential or burst write that started there).
 */
static inline enum cci_privsw_verdict cci_privsw_byte(
	const struct cci_privsw_sensor *s, struct cci_privsw_state *st,
	bool blocked, u16 reg, bool dataport, u8 *val)
{
	if (s->samsung_indirect) {
		if (dataport || reg == CCI_PRIVSW_SS_DATA ||
		    reg == CCI_PRIVSW_SS_DATA + 1) {
			bool high = dataport ? !(st->ss_ptr & 1) :
					       reg == CCI_PRIVSW_SS_DATA;
			u16 target = dataport ? st->ss_ptr :
					(st->ss_ptr & ~1) + !high;
			enum cci_privsw_verdict v = CCI_PRIVSW_PASS;

			if (st->ss_page == CCI_PRIVSW_SS_STD_PAGE)
				v = cci_privsw_reg_byte(s, st, blocked, target, val);
			/* The window moves on by one 16-bit word per word. */
			if (dataport)
				st->ss_ptr++;
			else if (!high)
				st->ss_ptr = (st->ss_ptr & ~1) + 2;
			return v;
		}
		if (reg == CCI_PRIVSW_SS_PAGE || reg == CCI_PRIVSW_SS_PAGE + 1) {
			cci_privsw_set_byte(&st->ss_page, reg == CCI_PRIVSW_SS_PAGE, *val);
			return CCI_PRIVSW_PASS;
		}
		if (reg == CCI_PRIVSW_SS_PTR || reg == CCI_PRIVSW_SS_PTR + 1) {
			cci_privsw_set_byte(&st->ss_ptr, reg == CCI_PRIVSW_SS_PTR, *val);
			return CCI_PRIVSW_PASS;
		}
	}
	return cci_privsw_reg_byte(s, st, blocked, reg, val);
}

/*
 * One write entry: @nbytes data bytes (1-4, most significant first) starting
 * at @reg. Returns the verdict and, in @data, the value to send.
 */
static inline enum cci_privsw_verdict cci_privsw_entry(
	const struct cci_privsw_sensor *s, struct cci_privsw_state *st,
	bool blocked, u16 reg, bool dataport, u8 nbytes, u32 *data)
{
	enum cci_privsw_verdict worst = CCI_PRIVSW_PASS, v;
	u32 out = 0;
	int k;

	for (k = 0; k < nbytes; k++) {
		int shift = 8 * (nbytes - 1 - k);
		u8 b = (*data >> shift) & 0xff;

		v = cci_privsw_byte(s, st, blocked, (u16)(reg + k), dataport, &b);
		if (v > worst)
			worst = v;
		out |= (u32)b << shift;
	}
	if (worst == CCI_PRIVSW_CHANGED)
		*data = out;
	return worst;
}

#endif /* _CAM_CCI_PRIVSW_CORE_H_ */
