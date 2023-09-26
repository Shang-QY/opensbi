/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#ifndef __SPM_H__
#define __SPM_H__

/** Representation of Secure Partition context */
struct sp_context {
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
};

/**
 * This function takes an SP context pointer and performs a synchronous entry
 * into it.
 * @param ctx pointer to SP context
 * @return 0 on success
 * @return other values decided by SP if it encounters an exception while running
 */
uint64_t spm_sp_synchronous_entry(struct sp_context *ctx);

/**
 * This function returns to the place where spm_sp_synchronous_entry() was
 * called originally.
 * @param ctx pointer to SP context
 * @param rc the return value for the original entry call
 */
void spm_sp_synchronous_exit(struct sp_context *ctx, uint64_t rc);

#endif
