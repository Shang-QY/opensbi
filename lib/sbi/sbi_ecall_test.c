/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Intel Corporation.
 *
 * Authors:
 *   Yong Li <yong.li@intel.com>
 */

#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_string.h>

#define SECURE_DOMAIN   "trusted-domain"

static int cm_entry_to_secure(void)
{
    int i;
    struct sbi_domain *dom, *tdom = NULL;
    sbi_domain_for_each(i, dom)
	{
		if (!sbi_strcmp(dom->name, SECURE_DOMAIN)) {
			tdom = dom;
            break;
		}
	}
    if (tdom)
	    sbi_domain_resume(tdom->index);
	return 0;
}

static int cm_exit_from_secure(void)
{
	sbi_domain_suspend(0);
	return 0;
}

static int sbi_ecall_test_handler(unsigned long extid, unsigned long funcid,
				  const struct sbi_trap_regs *regs,
				  unsigned long *out_val,
				  struct sbi_trap_info *out_trap)
{
	int ret = 0;

	switch (funcid) {
	case SBI_EXT_TEST_SECURE_ENTER:
		sbi_printf("sbi_ecall_test_handler: SBI_EXT_TEST_SECURE_ENTER\n");
		ret = cm_entry_to_secure();
		break;
	case SBI_EXT_TEST_SECURE_EXIT:
		sbi_printf("sbi_ecall_test_handler: SBI_EXT_TEST_SECURE_EXIT\n");
		ret = cm_exit_from_secure();
		break;
	default:
		ret = SBI_ENOTSUPP;
	}

	return ret;
}

struct sbi_ecall_extension ecall_test;

static int sbi_ecall_test_register_extensions(void)
{
	return sbi_ecall_register_extension(&ecall_test);
}

struct sbi_ecall_extension ecall_test = {
	.extid_start		= SBI_EXT_TEST,
	.extid_end		= SBI_EXT_TEST,
	.register_extensions	= sbi_ecall_test_register_extensions,
	.handle			= sbi_ecall_test_handler,
};
