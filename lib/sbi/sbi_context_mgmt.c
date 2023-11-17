/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_context_mgmt.h>

/** Assembly helpers */
uint64_t cpu_lowlevel_context_enter(struct sbi_trap_regs *regs, uint64_t *c_rt_ctx);
void cpu_lowlevel_context_exit(uint64_t c_rt_ctx, uint64_t ret);

uint64_t sbi_context_domain_context_enter(u32 index)
{
	uint64_t rc;
	struct sbi_domain *dom = sbi_domain_thishart_ptr();
	struct sbi_domain *tdom = domidx_to_domain_table[index];
	struct sbi_context_smode *ctx = tdom->next_ctx;
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);

	/* Switch to target domain */
	sbi_domain_assign_hart(tdom, current_hartid());
	for(int i = 0; i < pmp_count; i++) {
		pmp_disable(i);
	}
	sbi_hart_pmp_configure(scratch);

	/* Save current CSR context and setup Secure Partition's CSR context */
	ctx->csr_stvec    = csr_swap(CSR_STVEC, ctx->csr_stvec);
	ctx->csr_sscratch = csr_swap(CSR_SSCRATCH, ctx->csr_sscratch);
	ctx->csr_sie      = csr_swap(CSR_SIE, ctx->csr_sie);
	ctx->csr_satp     = csr_swap(CSR_SATP, ctx->csr_satp);

	rc = cpu_lowlevel_context_enter(&ctx->regs, &ctx->c_rt_ctx);

	/* Restore original domain */
	sbi_domain_assign_hart(dom, current_hartid());
	for(int i = 0; i < pmp_count; i++) {
		pmp_disable(i);
	}
	sbi_hart_pmp_configure(scratch);

	return rc;
}

void sbi_context_domain_context_exit(uint64_t rc)
{
	struct sbi_domain *dom = sbi_domain_thishart_ptr();
	struct sbi_context_smode *ctx = dom->next_ctx;

	if (!dom->context_mgmt_enabled || !ctx)
		return;

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
	ctx->csr_stvec    = csr_swap(CSR_STVEC, ctx->csr_stvec);
	ctx->csr_sscratch = csr_swap(CSR_SSCRATCH, ctx->csr_sscratch);
	ctx->csr_sie      = csr_swap(CSR_SIE, ctx->csr_sie);
	ctx->csr_satp     = csr_swap(CSR_SATP, ctx->csr_satp);

	/*
	 * The context manager must have initiated the original request through a
	 * synchronous entry into the domain context. Jump back to the
	 * original C runtime context with the value of rc in a0;
	 */
	cpu_lowlevel_context_exit(ctx->c_rt_ctx, rc);
}

static void domain_context_setup(struct sbi_domain *dom)
{
	unsigned long val;
	val = csr_read(CSR_MSTATUS);
	val = INSERT_FIELD(val, MSTATUS_MPP, dom->next_mode);
	val = INSERT_FIELD(val, MSTATUS_MPIE, 0);

	/* Setup secure M-mode CSR context */
	dom->next_ctx->regs.mstatus = val;
	dom->next_ctx->regs.mepc = dom->next_addr;

	/* Setup secure S-mode CSR context */
	dom->next_ctx->csr_stvec = dom->next_addr;
	dom->next_ctx->csr_sscratch = 0;
	dom->next_ctx->csr_sie = 0;
	dom->next_ctx->csr_satp = 0;

	/* Setup boot arguments */
	dom->next_ctx->regs.a0 = current_hartid();
	dom->next_ctx->regs.a1 = dom->next_arg1;
}

int sbi_context_mgmt_init(struct sbi_scratch *scratch)
{
	int rc;
	u32 i;
	struct sbi_domain *dom;

	sbi_domain_for_each(i, dom) {
		if(dom->context_mgmt_enabled) {
			/* Init domain */
			domain_context_setup(dom);

			/* Init domain */
			if ((rc = sbi_context_domain_context_enter(i)))
				return rc;
		}
	}
	return 0;
}
