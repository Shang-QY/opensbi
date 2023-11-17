/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#ifndef __SBI_CONTEXT_H__
#define __SBI_CONTEXT_H__

#include <sbi/sbi_types.h>
#include <sbi/sbi_trap.h>

struct sbi_context_smode {
	/** General registers, mepc and mstatus for trap state */
	struct sbi_trap_regs regs;
	/** S-mode CSR registers */
	uint64_t csr_stvec;
	uint64_t csr_sscratch;
	uint64_t csr_sie;
	uint64_t csr_satp;
	/**
	 * Stack address to restore M-mode C runtime context from after
	 * returning from a synchronous entry into domain context.
	 */
	uintptr_t c_rt_ctx;
};

uint64_t sbi_context_domain_context_enter(u32 domain_index);

void sbi_context_domain_context_exit(uint64_t rc);

int sbi_context_mgmt_init(struct sbi_scratch *scratch);

#endif
