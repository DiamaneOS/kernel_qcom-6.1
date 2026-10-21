/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 The DiamaneOS Project
 */
#ifndef __QRTR_IMSDCM_H_
#define __QRTR_IMSDCM_H_

#include <linux/types.h>
#include <linux/qrtr.h>
#include <linux/stddef.h>
#include <linux/kconfig.h>

struct sk_buff;
struct sock;

/* Bytes actually consumed by the control dispatchers. Check before tracing or
 * reading fields. Reused receive buffers are not an input-validation boundary.
 */
static inline size_t qrtr_ctrl_min_size(u32 command)
{
	switch (command) {
	case QRTR_TYPE_HELLO:
	case QRTR_TYPE_BYE:
	case QRTR_TYPE_EXIT:
	case QRTR_TYPE_PING:
		return sizeof(__le32);
	case QRTR_TYPE_NEW_LOOKUP:
	case QRTR_TYPE_DEL_LOOKUP:
		return offsetof(struct qrtr_ctrl_pkt, server.node);
	case QRTR_TYPE_DEL_CLIENT:
		return offsetof(struct qrtr_ctrl_pkt, client) +
			sizeof(((struct qrtr_ctrl_pkt *)0)->client);
	case QRTR_TYPE_NEW_SERVER:
	case QRTR_TYPE_DEL_SERVER:
	case QRTR_TYPE_RESUME_TX:
	case QRTR_TYPE_DEL_PROC:
		return sizeof(struct qrtr_ctrl_pkt);
	default:
		return 0;
	}
}

#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
#define QRTR_IMSDCM_SERVICE_ID 0x302
/* Downstream role allocation at the top of the ordinary ephemeral range.
 * Service discovery still advertises this port. It is not a modem wire requirement.
 */
#define QRTR_IMSDCM_PORT 0x7fff
bool qrtr_imsdcm_port_owned(u32 port);

/* Stock QMI service ID and the Android vendor_imsdcm allocation selected by
 * hardware/diamaneos/ims. Neither identity is supplied by the control payload.
 */
#define QRTR_IMSDCM_ANDROID_UID 2990

#define QRTR_IMSDCM_EPH_PORT_RANGE \
		XA_LIMIT(QRTR_MIN_EPH_SOCKET, QRTR_IMSDCM_PORT - 1)
#else
#define QRTR_IMSDCM_EPH_PORT_RANGE QRTR_EPH_PORT_RANGE
#endif

bool qrtr_imsdcm_ep_post_denied(u32 local_nid, u32 src_node, u32 src_port,
				u32 dst_port, u8 type, const void *payload,
				size_t size);
bool qrtr_imsdcm_bind_denied(struct sock *sk, u32 port, int zapped,
			     u32 bound_port);
bool qrtr_imsdcm_enqueue_denied(struct sock *sk, u32 port);
bool qrtr_imsdcm_sock_denied(struct sock *sk, u32 bound_port);
int qrtr_imsdcm_send_check(struct sock *sk, u32 bound_port, u32 dst_port);
int qrtr_imsdcm_ctrl_send_check(struct sock *sk, struct sk_buff *skb,
				u32 dst_port, u32 type, size_t len,
				u32 bound_node, u32 bound_port,
				struct qrtr_ctrl_pkt *pkt);

#endif
