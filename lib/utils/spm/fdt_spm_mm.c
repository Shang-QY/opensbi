/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/riscv_asm.h>
#include <sbi/sbi_bitops.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/spm/fdt_spm.h>
#include <sbi_utils/spm/spm.h>
#include <sbi_utils/mailbox/rpmi_msgprot.h>
#include <sbi/sbi_console.h>

#define MM_VERSION_MAJOR        1
#define MM_VERSION_MAJOR_SHIFT  16
#define MM_VERSION_MAJOR_MASK   0x7FFF
#define MM_VERSION_MINOR        0
#define MM_VERSION_MINOR_SHIFT  0
#define MM_VERSION_MINOR_MASK   0xFFFF
#define MM_VERSION_FORM(major, minor) ((major << MM_VERSION_MAJOR_SHIFT) | \
                                       (minor))
#define MM_VERSION_COMPILED     MM_VERSION_FORM(MM_VERSION_MAJOR, \
                                                MM_VERSION_MINOR)

static struct sp_context mm_context;

struct efi_param_header {
	uint8_t type;	/* type of the structure */
	uint8_t version; /* version of this structure */
	uint16_t size;   /* size of this structure in bytes */
	uint32_t attr;   /* attributes: unused bits SBZ */
};

struct efi_secure_partition_cpu_info {
	uint64_t mpidr;
	uint32_t linear_id;
	uint32_t flags;
};

struct efi_secure_partition_boot_info {
	struct efi_param_header header;
	uint64_t sp_mem_base;
	uint64_t sp_mem_limit;
	uint64_t sp_image_base;
	uint64_t sp_stack_base;
	uint64_t sp_heap_base;
	uint64_t sp_ns_comm_buf_base;
	uint64_t sp_shared_buf_base;
	uint64_t sp_image_size;
	uint64_t sp_pcpu_stack_size;
	uint64_t sp_heap_size;
	uint64_t sp_ns_comm_buf_size;
	uint64_t sp_shared_buf_size;
	uint32_t num_sp_mem_region;
	uint32_t num_cpus;
	struct efi_secure_partition_cpu_info *cpu_info;
};

struct efi_secure_shared_buffer {
	struct efi_secure_partition_boot_info mm_payload_boot_info;
	struct efi_secure_partition_cpu_info mm_cpu_info[1];
};

void set_mm_boot_args(struct sbi_trap_regs *regs)
{
	/* Fix me: where to setup boot_info for secure partition? */
	struct efi_secure_shared_buffer *mm_shared_buffer = (struct efi_secure_shared_buffer *)0x80B00000;

	mm_shared_buffer->mm_payload_boot_info.header.version = 0x01;
	mm_shared_buffer->mm_payload_boot_info.sp_mem_base	= 0x80C00000;
	mm_shared_buffer->mm_payload_boot_info.sp_mem_limit	= 0x82000000;
	mm_shared_buffer->mm_payload_boot_info.sp_image_base = 0x80C00000; // sp_mem_base
	mm_shared_buffer->mm_payload_boot_info.sp_stack_base =
		0x81FFFFFF; // sp_heap_base + sp_heap_size + SpStackSize
	mm_shared_buffer->mm_payload_boot_info.sp_heap_base =
		0x80F00000; // sp_mem_base + sp_image_size
	mm_shared_buffer->mm_payload_boot_info.sp_ns_comm_buf_base = 0xFFE00000;
	mm_shared_buffer->mm_payload_boot_info.sp_shared_buf_base = 0x81F80000;
	mm_shared_buffer->mm_payload_boot_info.sp_image_size	 = 0x300000;
	mm_shared_buffer->mm_payload_boot_info.sp_pcpu_stack_size = 0x10000;
	mm_shared_buffer->mm_payload_boot_info.sp_heap_size	 = 0x800000;
	mm_shared_buffer->mm_payload_boot_info.sp_ns_comm_buf_size = 0x200000;
	mm_shared_buffer->mm_payload_boot_info.sp_shared_buf_size = 0x80000;
	mm_shared_buffer->mm_payload_boot_info.num_sp_mem_region = 0x6;
	mm_shared_buffer->mm_payload_boot_info.num_cpus	 = 1;
	mm_shared_buffer->mm_cpu_info[0].linear_id		 = 0;
	mm_shared_buffer->mm_cpu_info[0].flags		 = 0;
	mm_shared_buffer->mm_payload_boot_info.cpu_info = mm_shared_buffer->mm_cpu_info;

	regs->a0 = current_hartid();
	regs->a1 = (uintptr_t)&mm_shared_buffer->mm_payload_boot_info;
}

/*
 * StandaloneMm early initialization.
 */
int spm_mm_init(void)
{
	int rc;

	/* clear pending interrupts */
	csr_read_clear(CSR_MIP, MIP_MTIP);
	csr_read_clear(CSR_MIP, MIP_STIP);
	csr_read_clear(CSR_MIP, MIP_SSIP);
	csr_read_clear(CSR_MIP, MIP_SEIP);

	unsigned long val = csr_read(CSR_MSTATUS);
	val = INSERT_FIELD(val, MSTATUS_MPP, PRV_S);
	val = INSERT_FIELD(val, MSTATUS_MPIE, 0);

	/* init secure context */
	mm_context.regs.mstatus = val;
	/* Fix me: entry point value in domain information of spm_mm */
	mm_context.regs.mepc = 0x80C00000;

	/* set boot arguments */
	set_mm_boot_args(&mm_context.regs);

	/* init secure CSR context */
	mm_context.csr_stvec = 0x80C00000;
	mm_context.csr_sscratch = 0;
	mm_context.csr_sie = 0;
	mm_context.csr_satp = 0;

	__asm__ __volatile__("sfence.vma" : : : "memory");

	rc = spm_sp_synchronous_entry(&mm_context);

	return rc;
}

static int spm_message_handler_mm(int srv_id,
				  void *tx, u32 tx_len,
				  void *rx, u32 rx_len,
				  unsigned long *ack_len)
{
	if (RPMI_MM_SRV_MM_VERSION == srv_id) {
		*((int32_t *)rx) = 0;
		*((uint32_t *)(rx + sizeof(uint32_t))) = MM_VERSION_COMPILED;
	} else if (RPMI_MM_SRV_MM_COMMUNICATE == srv_id) {
		spm_sp_synchronous_entry(&mm_context);
	} else if (RPMI_MM_SRV_MM_COMPLETE == srv_id) {
		spm_sp_synchronous_exit(&mm_context, 0);
	}
	return 0;
}

static const struct fdt_match fdt_spm_mm_match[] = {
	{ .compatible = "riscv,rpmi-spm-mm" },
	{ },
};

static const struct spm_chan spm_mm_chan = {
	.spm_message_handler = spm_message_handler_mm,
};

struct fdt_spm fdt_spm_mm = {
	.match_table = fdt_spm_mm_match,
	.init = spm_mm_init,
	.chan = spm_mm_chan
};
