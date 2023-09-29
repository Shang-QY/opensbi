/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/spm/fdt_spm.h>

/* List of FDT SPM service group managers generated at compile time */
extern struct fdt_spm *fdt_spm_service_groups[];
extern unsigned long fdt_spm_service_groups_size;

int fdt_spm_request_manager(void *fdt, int nodeoff, struct fdt_spm **out_manager)
{
	int pos;
	struct fdt_spm *drv;

	for (pos = 0; pos < fdt_spm_service_groups_size; pos++) {
		drv = fdt_spm_service_groups[pos];

		if (fdt_match_node(fdt, nodeoff, drv->match_table)) {
			*out_manager = drv;
			return SBI_SUCCESS;
		}
	}

	/* Platforms have no message handler for this SPM instance */
	return SBI_EFAIL;
}
