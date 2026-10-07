/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Microphone floor for the LPASS codec: while the privacy switch blocks the
 * microphones, every TX and VA decimator's PGA mute bit (TXn_TX_PATH_CTL bit 4)
 * is forced on in each register write. Kept free of driver state so KUnit can
 * test it (lpass-cdc-privsw-test.c).
 */

#ifndef LPASS_CDC_PRIVSW_H
#define LPASS_CDC_PRIVSW_H

#include <linux/types.h>
#include "lpass-cdc-registers.h"

/* The macro ids of lpass-cdc.h; lpass-cdc.c checks that they match. */
#define LPASS_CDC_PRIVSW_TX_MACRO	0
#define LPASS_CDC_PRIVSW_VA_MACRO	3

#define LPASS_CDC_PRIVSW_MUTE		0x10
#define LPASS_CDC_PRIVSW_DEC_STRIDE \
	(LPASS_CDC_TX1_TX_PATH_CTL - LPASS_CDC_TX0_TX_PATH_CTL)
#define LPASS_CDC_PRIVSW_TX_DECS	8
#define LPASS_CDC_PRIVSW_VA_DECS	4

/* Whether @reg (an offset in macro @macro_id) is a decimator's TX_PATH_CTL. */
static inline bool lpass_cdc_privsw_is_dec_ctl(u16 macro_id, u16 reg)
{
	u16 first, count;

	switch (macro_id) {
	case LPASS_CDC_PRIVSW_TX_MACRO:
		first = LPASS_CDC_TX0_TX_PATH_CTL - TX_START_OFFSET;
		count = LPASS_CDC_PRIVSW_TX_DECS;
		break;
	case LPASS_CDC_PRIVSW_VA_MACRO:
		first = LPASS_CDC_VA_TX0_TX_PATH_CTL - VA_START_OFFSET;
		count = LPASS_CDC_PRIVSW_VA_DECS;
		break;
	default:
		return false;
	}
	if (reg < first)
		return false;
	reg -= first;
	return reg % LPASS_CDC_PRIVSW_DEC_STRIDE == 0 &&
	       reg / LPASS_CDC_PRIVSW_DEC_STRIDE < count;
}

/* The value that reaches the hardware for a write of @val to @reg. */
static inline u8 lpass_cdc_privsw_filter(bool muted, u16 macro_id, u16 reg,
					 u8 val)
{
	if (muted && lpass_cdc_privsw_is_dec_ctl(macro_id, reg))
		return val | LPASS_CDC_PRIVSW_MUTE;
	return val;
}

/* The regmap address of decimator @n's TX_PATH_CTL, @n counting TX then VA. */
static inline unsigned int lpass_cdc_privsw_dec_ctl_reg(unsigned int n)
{
	if (n < LPASS_CDC_PRIVSW_TX_DECS)
		return LPASS_CDC_TX0_TX_PATH_CTL + n * LPASS_CDC_PRIVSW_DEC_STRIDE;
	n -= LPASS_CDC_PRIVSW_TX_DECS;
	return LPASS_CDC_VA_TX0_TX_PATH_CTL + n * LPASS_CDC_PRIVSW_DEC_STRIDE;
}

#define LPASS_CDC_PRIVSW_ALL_DECS \
	(LPASS_CDC_PRIVSW_TX_DECS + LPASS_CDC_PRIVSW_VA_DECS)

#endif /* LPASS_CDC_PRIVSW_H */
