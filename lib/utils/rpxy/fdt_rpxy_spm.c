/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_rpxy.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/rpxy/fdt_rpxy.h>
#include <sbi_utils/spm/fdt_spm.h>
#include <sbi_utils/mailbox/rpmi_msgprot.h>
#include <sbi/sbi_console.h>

struct rpxy_spm_data {
	u32 service_group_id;
	int num_services;
	struct sbi_rpxy_service *services;
};

struct rpxy_spm {
	struct sbi_rpxy_service_group group;
	struct spm_chan chan;
};

static int rpxy_spm_send_message(struct sbi_rpxy_service_group *grp,
				  struct sbi_rpxy_service *srv,
				  void *tx, u32 tx_len,
				  void *rx, u32 rx_len,
				  unsigned long *ack_len)
{
	int ret;
	struct rpxy_spm *rspm = container_of(grp, struct rpxy_spm, group);

	ret = rspm->chan.spm_message_handler(srv->id, tx, tx_len, rx, rx_len, ack_len);

	return ret;
}

static int rpxy_spm_init(void *fdt, int nodeoff,
			  const struct fdt_match *match)
{
	int rc;
	struct rpxy_spm *rspm;
	struct spm_chan chan;
	const struct rpxy_spm_data *data = match->data;

	/* Allocate context for RPXY mbox client */
	rspm = sbi_zalloc(sizeof(*rspm));
	if (!rspm)
		return SBI_ENOMEM;

	/* Request SPM service group message handler */
	rc = fdt_spm_request_chan(fdt, nodeoff, &chan);
	if (rc) {
		sbi_free(rspm);
		return 0;
	}

	/* Setup RPXY spm client */
	rspm->group.transport_id = 0;
	rspm->group.service_group_id = data->service_group_id;
	rspm->group.max_message_data_len = -1;
	rspm->group.num_services = data->num_services;
	rspm->group.services = data->services;
	rspm->group.send_message = rpxy_spm_send_message;
	rspm->chan = chan;

	/* Register RPXY service group */
	rc = sbi_rpxy_register_service_group(&rspm->group);
	if (rc) {
		sbi_free(rspm);
		return rc;
	}

	return 0;
}

static struct sbi_rpxy_service mm_services[] = {
{
	.id = RPMI_MM_SRV_MM_VERSION,
	.min_tx_len = 0,
	.max_tx_len = 0,
	.min_rx_len = sizeof(u32),
	.max_rx_len = sizeof(u32),
},
{
	.id = RPMI_MM_SRV_MM_COMMUNICATE,
	.min_tx_len = 0,
	.max_tx_len = 0x1000,
	.min_rx_len = 0,
	.max_rx_len = 0,
},
{
	.id = RPMI_MM_SRV_MM_COMPLETE,
	.min_tx_len = 0,
	.max_tx_len = 0x1000,
	.min_rx_len = 0,
	.max_rx_len = 0,
},
};

static struct rpxy_spm_data mm_data = {
	.service_group_id = RPMI_SRVGRP_SPM_MM,
	.num_services = array_size(mm_services),
	.services = mm_services,
};

static const struct fdt_match rpxy_spm_match[] = {
	{ .compatible = "riscv,rpmi-spm-mm", .data = &mm_data }, 
	{},
};

struct fdt_rpxy fdt_rpxy_spm = {
	.match_table = rpxy_spm_match,
	.init = rpxy_spm_init,
};
