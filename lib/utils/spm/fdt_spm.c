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

int fdt_spm_init(void)
{
	int pos, noff, rc;
	struct fdt_spm *drv;
	const struct fdt_match *match;
	void *fdt = fdt_get_address();

	for (pos = 0; pos < fdt_spm_service_groups_size; pos++) {
		drv = fdt_spm_service_groups[pos];

		noff = -1;
		while ((noff = fdt_find_match(fdt, noff,
					drv->match_table, &match)) >= 0) {
			/* drv->init must not be NULL */
			if (drv->init == NULL)
				return SBI_EFAIL;

			rc = drv->init(fdt, noff, match);
			if (rc == SBI_ENODEV)
				continue;
			if (rc)
				return rc;

			/*
			 * We may have multiple SPM service group instances
			 * of the same type, managed by the same service group
			 * instance manager, so we cannot break here.
			 */
		}
	}

	/* Platforms might not have any RPXY devices so don't fail */
	return 0;
}

int fdt_spm_request_chan(void *fdt, int nodeoff, struct spm_chan *out_chan)
{
	int pos, rc;
	struct fdt_spm *drv;

	for (pos = 0; pos < fdt_spm_service_groups_size; pos++) {
		drv = fdt_spm_service_groups[pos];

		if (fdt_match_node(fdt, nodeoff, drv->match_table)) {
			*out_chan = drv->chan;
			return SBI_SUCCESS;
		}
	}

	/* Platforms have no message handler for this SPM instance */
	return SBI_EFAIL;
}
