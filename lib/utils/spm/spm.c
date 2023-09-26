/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/sbi_trap.h>
#include <sbi/riscv_asm.h>
#include <sbi_utils/spm/spm.h>

/*
 * Save current CSR registers context and restore original context.
 */
static void save_restore_csr_context(struct sp_context *ctx)
{
	uint64_t tmp;

	tmp = ctx->csr_stvec;
	ctx->csr_stvec = csr_read(CSR_STVEC);
	csr_write(CSR_STVEC, tmp);

	tmp = ctx->csr_sscratch;
	ctx->csr_sscratch = csr_read(CSR_SSCRATCH);
	csr_write(CSR_SSCRATCH, tmp);

	tmp = ctx->csr_sie;
	ctx->csr_sie = csr_read(CSR_SIE);
	csr_write(CSR_SIE, tmp);

	tmp = ctx->csr_satp;
	ctx->csr_satp = csr_read(CSR_SATP);
	csr_write(CSR_SATP, tmp);
}

/** Assembly helpers */
uint64_t spm_secure_partition_enter(struct sbi_trap_regs *regs, uintptr_t *c_rt_ctx);
void spm_secure_partition_exit(uint64_t c_rt_ctx, uint64_t ret);

uint64_t spm_sp_synchronous_entry(struct sp_context *ctx)
{
	uint64_t rc;

	/* Save current CSR context and setup Secure Partition's CSR context */
	save_restore_csr_context(ctx);

	/* Enter Secure Partition */
	rc = spm_secure_partition_enter(&ctx->regs, &ctx->c_rt_ctx);

	return rc;
}

void spm_sp_synchronous_exit(struct sp_context *ctx, uint64_t rc)
{
	/* Save secure state */
	uintptr_t *prev = (uintptr_t *)&ctx->regs;
	uintptr_t *trap_regs = (uintptr_t *)(csr_read(CSR_MSCRATCH) - SBI_TRAP_REGS_SIZE);
	for (int i = 0; i < SBI_TRAP_REGS_SIZE / __SIZEOF_POINTER__; ++i) {
		prev[i] = trap_regs[i];
	}

	/* Set SBI Err and Ret */
	ctx->regs.a0 = SBI_SUCCESS;
	ctx->regs.a1 = 0;

	/* Set MEPC to next instruction */
	ctx->regs.mepc = ctx->regs.mepc + 4;

	/* Save Secure Partition's CSR context and restore original CSR context */
	save_restore_csr_context(ctx);

	/*
	 * The SPM must have initiated the original request through a
	 * synchronous entry into the secure partition. Jump back to the
	 * original C runtime context with the value of rc in a0;
	 */
	spm_secure_partition_exit(ctx->c_rt_ctx, rc);
}

