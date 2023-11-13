/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Intel Corporation.
 *
 * Authors:
 *   Yong Li <yong.li@intel.com>
 */

#ifndef __SBI_CONTEXT_H__
#define __SBI_CONTEXT_H__

#include <sbi/sbi_types.h>
#include <sbi/sbi_trap.h>

typedef struct sbi_context_smode {
	/** secure context for all general registers */
	struct sbi_trap_regs regs;
	/** secure context for S mode CSR registers */
	uint64_t csr_stvec;
	uint64_t csr_sscratch;
	uint64_t csr_sie;
	uint64_t csr_satp;
	/**
	 * stack address to restore C runtime context from after
	 * returning from a synchronous entry into Secure Partition.
	 */
	uintptr_t c_rt_ctx;
} sbi_context_smode_t;

uint64_t sbi_context_smode_enter(u32 domain_index) ;

void sbi_context_smode_exit(uint64_t rc);

int sbi_context_mgmr_init(struct sbi_scratch *scratch);

#endif