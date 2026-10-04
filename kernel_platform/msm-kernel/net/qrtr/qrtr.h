/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __QRTR_H_
#define __QRTR_H_

#include <linux/types.h>
#include <linux/qrtr.h>
#include <linux/stddef.h>
#include <linux/kconfig.h>

/* Bytes actually consumed by the control dispatchers. Check before tracing or
 * reading fields; reused receive buffers are not an input-validation boundary.
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
 * Service discovery still advertises this port; it is not a modem wire requirement.
 */
#define QRTR_IMSDCM_PORT 0x7fff
bool qrtr_imsdcm_port_owned(u32 port);
#endif

struct sk_buff;

/* endpoint node id auto assignment */
#define QRTR_EP_NID_AUTO (-1)
#define QRTR_EP_NET_ID_AUTO (1)

#define QRTR_DEL_PROC_MAGIC	0xe111

/**
 * struct qrtr_endpoint - endpoint handle
 * @xmit: Callback for outgoing packets
 *
 * The socket buffer passed to the xmit function becomes owned by the endpoint
 * driver.  As such, when the driver is done with the buffer, it should
 * call kfree_skb() on failure, or consume_skb() on success.
 */
struct qrtr_endpoint {
	int (*xmit)(struct qrtr_endpoint *ep, struct sk_buff *skb);
	/* private: not for endpoint use */
	struct qrtr_node *node;
};

/**
 * struct qrtr_array - array with size
 * @arr: elements in the array
 * @size: number of elements
 *
 * An array with its size provided.
 */
struct qrtr_array {
	u32 *arr;
	size_t size;
};

int qrtr_endpoint_register(struct qrtr_endpoint *ep, unsigned int net_id,
			   bool rt, struct qrtr_array *no_wake);

void qrtr_endpoint_unregister(struct qrtr_endpoint *ep);

int qrtr_endpoint_post(struct qrtr_endpoint *ep, const void *data, size_t len);

int qrtr_ns_init(void);

void qrtr_ns_remove(void);

int qrtr_peek_pkt_size(const void *data);

int qrtr_get_service_id(unsigned int node_id, unsigned int port_id);

void qrtr_print_wakeup_reason(const void *data);
#endif
