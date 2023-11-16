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
	/** secure domain context for all general registers */
	struct sbi_trap_regs regs;
	/** secure domain context for S mode CSR registers */
	uint64_t csr_stvec;
	uint64_t csr_sscratch;
	uint64_t csr_sie;
	uint64_t csr_satp;
	/**
	 * stack address to restore M-mode runtime context from after
	 * returning from a synchronous entry into domain context.
	 */
	uintptr_t c_rt_ctx;
};

uint64_t sbi_context_smode_enter(u32 domain_index) ;

void sbi_context_smode_exit(uint64_t rc);

int sbi_context_mgmt_init(struct sbi_scratch *scratch);

#endif
