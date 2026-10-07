/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Camera floor: while the privacy switch blocks the cameras, every known
 * image sensor sends its test pattern, whatever userspace writes over the
 * CCI. See cam_cci_privsw.c.
 */

#ifndef _CAM_CCI_PRIVSW_H_
#define _CAM_CCI_PRIVSW_H_

#include "cam_cci_dev.h"

int cam_cci_privsw_init(void);
void cam_cci_privsw_exit(void);
void cam_cci_privsw_add(struct cci_device *cci_dev);
void cam_cci_privsw_remove(struct cci_device *cci_dev);

/*
 * Before a userspace write: 0 to send it (with *copy, if set, holding the
 * values to send instead; the caller frees it), 1 to drop it and report
 * success, <0 on error.
 */
int cam_cci_privsw_filter(struct cci_device *cci_dev, struct cam_cci_ctrl *ctrl,
	struct cam_sensor_i2c_reg_array **copy);

/*
 * After a userspace write to a protected sensor while blocked: fills @out
 * (using @regs, CCI_PRIVSW_PATTERN_MAX entries) with the pattern write that
 * follows it. Returns false when nothing follows.
 */
bool cam_cci_privsw_pattern(struct cci_device *cci_dev,
	const struct cam_cci_ctrl *ctrl, struct cam_cci_ctrl *out,
	struct cam_sensor_i2c_reg_array *regs);

/* In cam_cci_core.c: a write the floor issues itself. */
int32_t cam_cci_privsw_write(struct cci_device *cci_dev,
	struct cam_cci_ctrl *ctrl);

#endif /* _CAM_CCI_PRIVSW_H_ */
