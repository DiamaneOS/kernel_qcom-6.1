/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * Test builds only (CONFIG_SPECTRA_CDM_REACH_PROBE, never in a release
 * configuration): does the TFE's real-time CDM reach registers outside the
 * TFE wrapper, the CCI in particular? See cam_tfe_cdm_reach_probe.c.
 */

#ifndef _CAM_TFE_CDM_REACH_PROBE_H_
#define _CAM_TFE_CDM_REACH_PROBE_H_

#include "cam_cdm_intf_api.h"

#if IS_ENABLED(CONFIG_SPECTRA_CDM_REACH_PROBE)
/* Appends the probe to @cdm_cmd (with @count entries) once armed; returns the new count. */
uint32_t cam_tfe_cdm_reach_probe(struct cam_cdm_bl_request *cdm_cmd,
	uint32_t count, uint32_t max, int32_t smmu_hdl);
#else
static inline uint32_t cam_tfe_cdm_reach_probe(struct cam_cdm_bl_request *cdm_cmd,
	uint32_t count, uint32_t max, int32_t smmu_hdl)
{
	return count;
}
#endif

#endif /* _CAM_TFE_CDM_REACH_PROBE_H_ */
