/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * The privacy switch's state machine, without hardware, so KUnit can test it.
 */

#ifndef _PRIVSW_CORE_H
#define _PRIVSW_CORE_H

#include <linux/bits.h>
#include <linux/diamaneos_privsw.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#define PRIVSW_ALL	GENMASK(PRIVSW_NR_SENSORS - 1, 0)

struct privsw_core {
	u32 policy;	/* sensors armed by the boot-time write */
	bool sealed;	/* the policy was written; nothing changes it until reboot */
	u32 enforced;	/* sensors with a client in the kernel; only grows */
	bool down;	/* the switch is in the blocking position */
};

/* Before any GPIO read and before the policy write: everything blocked. */
static inline void privsw_core_init(struct privsw_core *c)
{
	c->policy = 0;
	c->sealed = false;
	c->enforced = 0;
	c->down = true;
}

/* Sensors the switch blocks this boot. Unsealed means all (fail closed). */
static inline u32 privsw_core_armed(const struct privsw_core *c)
{
	return c->sealed ? c->policy : PRIVSW_ALL;
}

/* Sensors blocked right now. */
static inline u32 privsw_core_blocked(const struct privsw_core *c)
{
	return c->down ? privsw_core_armed(c) : 0;
}

/* What the kernel itself enforces this boot: armed and with a client. */
static inline u32 privsw_core_enforced(const struct privsw_core *c)
{
	return privsw_core_armed(c) & c->enforced;
}

/*
 * The one policy write: a decimal mask of enum privsw_sensor bits. Accepted
 * once; any later write fails with -EPERM, whatever it holds. A malformed
 * write fails with -EINVAL and does not seal, so the policy stays "all".
 */
static inline int privsw_core_seal(struct privsw_core *c, const char *buf,
				   size_t len)
{
	char tmp[12];
	u32 value;
	int ret;

	if (c->sealed)
		return -EPERM;
	if (len == 0 || len >= sizeof(tmp))
		return -EINVAL;
	memcpy(tmp, buf, len);
	tmp[len] = '\0';
	ret = kstrtou32(tmp, 10, &value);
	if (ret)
		return -EINVAL;
	if (value & ~PRIVSW_ALL)
		return -EINVAL;
	c->policy = value;
	c->sealed = true;
	return 0;
}

/*
 * The switch position from a GPIO read (the logical value, 0 or 1): blocking
 * when it equals @blocking_value. A failed read counts as blocking.
 */
static inline bool privsw_core_level_down(int level, int blocking_value)
{
	if (level < 0)
		return true;
	return !!level == !!blocking_value;
}

/*
 * Debounce, asymmetric in favour of privacy: a move into the blocking
 * position applies at once; a move out of it only once the level has been
 * stable for the debounce time (the caller re-reads then and asks again with
 * settled = true).
 */
static inline bool privsw_core_apply_now(bool down, bool settled)
{
	return down || settled;
}

#endif /* _PRIVSW_CORE_H */
