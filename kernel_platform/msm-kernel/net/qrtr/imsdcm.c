// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 The DiamaneOS Project
 *
 * The IMS DCM publisher role and the local node checks of af_qrtr.c.
 */

#include <linux/cred.h>
#include <linux/security.h>
#include <linux/skbuff.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>
#include <net/sock.h>

#include "imsdcm.h"

#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
static bool qrtr_imsdcm_publisher_allowed(struct sock *sk)
{
	static const char subject[] = "u:r:diamaneos_imsdcm:s0";
	kuid_t owner = make_kuid(&init_user_ns, QRTR_IMSDCM_ANDROID_UID);
	char *context = NULL;
	u32 secid = 0, length = 0;
	bool allowed = false;

	/* Only sockets created by the kernel may forward name-service records.
	 * A userspace sender cannot set this socket flag or forge its subject.
	 */
	if (sk->sk_kern_sock)
		return true;
	if (!uid_eq(current_uid(), owner) || !uid_eq(sock_i_uid(sk), owner))
		return false;
	security_current_getsecid_subj(&secid);
	if (security_secid_to_secctx(secid, &context, &length))
		return false;
	if (context && (length == sizeof(subject) ||
			length == sizeof(subject) - 1))
		allowed = !memcmp(context, subject, sizeof(subject) - 1) &&
			(length == sizeof(subject) - 1 ||
			 context[sizeof(subject) - 1] == '\0');
	if (context)
		security_release_secctx(context, length);
	return allowed;
}
#endif

bool qrtr_imsdcm_ep_post_denied(u32 local_nid, u32 src_node, u32 src_port,
				u32 dst_port, u8 type, const void *payload,
				size_t size)
{
	/* An endpoint may bridge other remote nodes, but it can never impersonate
	 * this AP's source identity or use its name-service port-zero alias.
	 */
	if (src_node == local_nid || src_node == QRTR_NODE_BCAST ||
	    !src_port || !dst_port)
		return true;
	if (dst_port == QRTR_PORT_CTRL || type == QRTR_TYPE_DEL_PROC ||
	    type == QRTR_TYPE_RESUME_TX) {
		const struct qrtr_ctrl_pkt *control = payload;
		u32 command;

		if (size < sizeof(control->cmd))
			return true;
		command = le32_to_cpu(control->cmd);
		if (command != type || !qrtr_ctrl_min_size(command) ||
		    size < qrtr_ctrl_min_size(command))
			return true;
		if ((command == QRTR_TYPE_NEW_SERVER || command == QRTR_TYPE_DEL_SERVER) &&
		    le32_to_cpu(control->server.node) == local_nid)
			return true;
		if (command == QRTR_TYPE_DEL_CLIENT &&
		    le32_to_cpu(control->client.node) == local_nid)
			return true;
		if (command == QRTR_TYPE_DEL_PROC &&
		    le32_to_cpu(control->proc.node) == local_nid)
			return true;
	}
	return false;
}

bool qrtr_imsdcm_bind_denied(struct sock *sk, u32 port, int zapped,
			     u32 bound_port)
{
#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
	/* The reserved role owns this port even while unbound. Protect every
	 * socket created by the role, including a borrowed FD before first bind.
	 */
	if (port == QRTR_IMSDCM_PORT ||
	    __kuid_val(sock_i_uid(sk)) == QRTR_IMSDCM_ANDROID_UID ||
	    (!zapped && bound_port == QRTR_IMSDCM_PORT)) {
		if (!qrtr_imsdcm_publisher_allowed(sk))
			return true;
		/* A receive queue must never escape the reserved role via rebind. */
		if (!zapped && bound_port == QRTR_IMSDCM_PORT &&
		    port != QRTR_IMSDCM_PORT)
			return true;
	}
#endif
	return false;
}

bool qrtr_imsdcm_enqueue_denied(struct sock *sk, u32 port)
{
#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
	/* Modem data reaches the reserved DCM port through the node RX path.
	 * Locally, only kernel sockets and the role itself may queue to it.
	 * Ownerless kernel packets (sk == NULL) may come from atomic context.
	 */
	if (port == QRTR_IMSDCM_PORT && sk &&
	    !qrtr_imsdcm_publisher_allowed(sk))
		return true;
#endif
	return false;
}

bool qrtr_imsdcm_sock_denied(struct sock *sk, u32 bound_port)
{
#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
	if (!sk->sk_kern_sock &&
	    (bound_port == QRTR_IMSDCM_PORT ||
	     __kuid_val(sock_i_uid(sk)) == QRTR_IMSDCM_ANDROID_UID) &&
	    !qrtr_imsdcm_publisher_allowed(sk))
		return true;
#endif
	return false;
}

int qrtr_imsdcm_send_check(struct sock *sk, u32 bound_port, u32 dst_port)
{
	if (qrtr_imsdcm_sock_denied(sk, bound_port))
		return -EPERM;

	/* Port zero is the internal control-socket xarray key, never a wire
	 * destination. Reject it for both addressed and connected sockets.
	 */
	if (!dst_port)
		return -EINVAL;
	return 0;
}

int qrtr_imsdcm_ctrl_send_check(struct sock *sk, struct sk_buff *skb,
				u32 dst_port, u32 type, size_t len,
				u32 bound_node, u32 bound_port,
				struct qrtr_ctrl_pkt *pkt)
{
	if (dst_port == QRTR_PORT_CTRL && !sk->sk_kern_sock) {
		size_t minimum = qrtr_ctrl_min_size(type);

		if (!minimum || len < minimum || type == QRTR_TYPE_BYE ||
		    type == QRTR_TYPE_DEL_PROC)
			return -EINVAL;
		if (type == QRTR_TYPE_DEL_CLIENT) {
			struct qrtr_ctrl_pkt client = { 0 };

			if (skb_copy_bits(skb, 0, &client, minimum) ||
			    le32_to_cpu(client.client.node) != bound_node ||
			    le32_to_cpu(client.client.port) != bound_port)
				return -EINVAL;
		}
#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
		if (type == QRTR_TYPE_DEL_CLIENT && bound_port == QRTR_IMSDCM_PORT &&
		    !qrtr_imsdcm_publisher_allowed(sk))
			return -EPERM;
#endif
	}
	if (dst_port == QRTR_PORT_CTRL &&
	    (type == QRTR_TYPE_NEW_SERVER || type == QRTR_TYPE_DEL_SERVER)) {
		/* Do not inspect or forward a truncated server record. */
		if (len < sizeof(*pkt) || skb_copy_bits(skb, 0, pkt, sizeof(*pkt)))
			return -EINVAL;
		if (!sk->sk_kern_sock) {
			pkt->server.node = cpu_to_le32(bound_node);
			pkt->server.port = cpu_to_le32(bound_port);
			if (skb_store_bits(skb, 0, pkt, sizeof(*pkt)))
				return -EINVAL;
		}
#if IS_ENABLED(CONFIG_QRTR_IMSDCM_OWNERSHIP)
		if ((le32_to_cpu(pkt->server.service) == QRTR_IMSDCM_SERVICE_ID ||
		     bound_port == QRTR_IMSDCM_PORT) &&
		    !qrtr_imsdcm_publisher_allowed(sk))
			return -EPERM;
		if (!sk->sk_kern_sock &&
		    le32_to_cpu(pkt->server.service) == QRTR_IMSDCM_SERVICE_ID &&
		    bound_port != QRTR_IMSDCM_PORT)
			return -EPERM;
#endif
	}
	return 0;
}
