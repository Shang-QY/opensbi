/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <libfdt.h>
#include <libfdt_env.h>
#include <sbi/sbi_math.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_hart.h>
#include <sbi_utils/spm/spm.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi/sbi_console.h>

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

static void pmp_disable_all(struct sbi_scratch *scratch)
{
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);
	for (int i = 0; i < pmp_count; i++) {
		pmp_disable(i);
	}
}

static void spm_sp_oldpmp_configure(struct sbi_scratch *scratch,
				     unsigned int pmp_count,
				     unsigned int pmp_gran_log2,
				     unsigned long pmp_addr_max,
				     struct sbi_domain *dom)
{
	struct sbi_domain_memregion *reg;
	unsigned int pmp_idx = 0;
	unsigned int pmp_flags;
	unsigned long pmp_addr;

	sbi_domain_for_each_memregion(dom, reg) {
		if (pmp_count <= pmp_idx)
			break;

		pmp_flags = 0;

		/*
		 * If permissions are to be enforced for all modes on
		 * this region, the lock bit should be set.
		 */
		if (reg->flags & SBI_DOMAIN_MEMREGION_ENF_PERMISSIONS)
			pmp_flags |= PMP_L;

		if (reg->flags & SBI_DOMAIN_MEMREGION_SU_READABLE)
			pmp_flags |= PMP_R;
		if (reg->flags & SBI_DOMAIN_MEMREGION_SU_WRITABLE)
			pmp_flags |= PMP_W;
		if (reg->flags & SBI_DOMAIN_MEMREGION_SU_EXECUTABLE)
			pmp_flags |= PMP_X;

		pmp_addr = reg->base >> PMP_SHIFT;
		if (pmp_gran_log2 <= reg->order && pmp_addr < pmp_addr_max) {
			pmp_set(pmp_idx++, pmp_flags, reg->base, reg->order);
		};
	}
}

static void spm_sp_pmp_configure(struct sbi_scratch *scratch, struct sbi_domain *dom)
{
	unsigned int pmp_bits, pmp_gran_log2;
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);
	unsigned long pmp_addr_max;

	pmp_gran_log2 = log2roundup(sbi_hart_pmp_granularity(scratch));
	pmp_bits = sbi_hart_pmp_addrbits(scratch) - 1;
	pmp_addr_max = (1UL << pmp_bits) | ((1UL << pmp_bits) - 1);

	spm_sp_oldpmp_configure(scratch, pmp_count,
						pmp_gran_log2, pmp_addr_max, dom);

	__asm__ __volatile__("sfence.vma");
}

/** Assembly helpers */
uint64_t spm_secure_partition_enter(struct sbi_trap_regs *regs, uintptr_t *c_rt_ctx);
void spm_secure_partition_exit(uint64_t c_rt_ctx, uint64_t ret);

uint64_t spm_sp_synchronous_entry(struct sp_context *ctx)
{
	uint64_t rc;
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();

	/* Switch to SP domain*/
	pmp_disable_all(scratch);
	spm_sp_pmp_configure(scratch, ctx->dom);

	/* Save current CSR context and setup Secure Partition's CSR context */
	save_restore_csr_context(ctx);

	/* Enter Secure Partition */
	rc = spm_secure_partition_enter(&ctx->regs, &ctx->c_rt_ctx);

	/* Restore original domain */
	pmp_disable_all(scratch);
	sbi_hart_pmp_configure(scratch);

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

int spm_sp_find_domain(void *fdt, int nodeoff, struct sbi_domain **output_domain)
{
	const u32 *val;
	int domain_offset, len;
	char name[64];
	u32 i;
	struct sbi_domain *dom;

	val = fdt_getprop(fdt, nodeoff, "opensbi-domain", &len);
	if (!val || len < 4) {
		return SBI_EINVAL;
	}

	domain_offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*val));
	if (domain_offset < 0) {
		return SBI_EINVAL;
	}

	/* Read DT node name and find match */
	strncpy(name, fdt_get_name(fdt, domain_offset, NULL),
			sizeof(name));
	name[sizeof(name) - 1] = '\0';

	sbi_domain_for_each(i, dom) {
		if (!sbi_strcmp(dom->name, name)) {
			*output_domain = dom;
			return SBI_SUCCESS;
		}
	}

	return SBI_EINVAL;
}
