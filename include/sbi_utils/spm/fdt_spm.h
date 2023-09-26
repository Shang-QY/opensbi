/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#ifndef __FDT_SPM_H__
#define __FDT_SPM_H__

#include <sbi/sbi_types.h>

struct fdt_spm {
	const struct fdt_match *match_table;
	int (*setup)(void *fdt, int nodeoff,
			const struct fdt_match *match);
	int (*init)(void);
	int (*spm_message_handler)(int srv_id,
			void *tx, u32 tx_len,
			void *rx, u32 rx_len,
			unsigned long *ack_len);
};

/** Request the manager corresponding to an SPM service group instance */
int fdt_spm_request_manager(void *fdt, int nodeoff, struct fdt_spm **out_manager);

#endif
