/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#ifndef __FDT_SPM_H__
#define __FDT_SPM_H__

#include <sbi/sbi_types.h>

#ifdef CONFIG_FDT_SPM

struct spm_chan {
	int (*spm_message_handler)(int srv_id,
			void *tx, u32 tx_len,
			void *rx, u32 rx_len,
			unsigned long *ack_len);
};

struct fdt_spm {
	const struct fdt_match *match_table;
	int (*init)(void);
    struct spm_chan chan;
};

/** Request the message handler corresponding to an SPM service group instance */
int fdt_spm_request_chan(void *fdt, int nodeoff, struct spm_chan *out_chan);

int fdt_spm_init(void);

#else

static inline int fdt_spm_init(void) { return 0; }

#endif

#endif
