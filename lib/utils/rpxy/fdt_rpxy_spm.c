/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_rpxy.h>
#include <sbi/sbi_domain.h>
#include <libfdt.h>

#include <sbi_utils/rpxy/fdt_rpxy.h>
#include <sbi/sbi_console.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_bitops.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/mailbox/rpmi_msgprot.h>
#include <sbi/sbi_console.h>

struct fdt_spm {
	const struct fdt_match *match_table;
	int (*setup)(void *fdt, int nodeoff,
			const struct fdt_match *match);
	int (*spm_message_handler)(int srv_id,
			void *tx, u32 tx_len,
			void *rx, u32 rx_len,
			unsigned long *ack_len);
};

/** Request the manager corresponding to an SPM service group instance */
int fdt_spm_request_manager(void *fdt, int nodeoff, struct fdt_spm **out_manager);

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

static struct sbi_dynamic_domain *dd;

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

void set_mm_boot_info(uint64_t a1)
{
	struct efi_secure_shared_buffer *mm_shared_buffer = (struct efi_secure_shared_buffer *)a1;

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
}

int find_dynamic_domain(void *fdt, int nodeoff, struct sbi_dynamic_domain **output_dd)
{
	const u32 *val;
	int domain_offset, len;
	char name[64];

	val = fdt_getprop(fdt, nodeoff, "opensbi-dynamic-domain", &len);
	if (!val || len < 4) {
		return SBI_EINVAL;
	}

	domain_offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*val));
	if (domain_offset < 0) {
		return SBI_EINVAL;
	}

	val = fdt_getprop(fdt, domain_offset, "domain-instance", &len);
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

	return sbi_find_dynamic_domain(name, output_dd);
}

/*
 * Initialize StandaloneMm SP context.
 */
int spm_mm_setup(void *fdt, int nodeoff,
			  const struct fdt_match *match)
{
	int rc;

	rc = find_dynamic_domain(fdt, nodeoff, &dd);
	if (rc) {
        sbi_printf("[SQY debug] Error: %s %d\n", __func__, __LINE__);
		return SBI_EINVAL;
	}

	set_mm_boot_info(dd->dom->next_arg1);

	return 0;
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
		sbi_dynamic_domain_entry(dd);
	} else if (RPMI_MM_SRV_MM_COMPLETE == srv_id) {
		sbi_dynamic_domain_exit(dd, 0);
	}
	return 0;
}

static const struct fdt_match fdt_spm_mm_match[] = {
	{ .compatible = "riscv,rpmi-spm-mm" },
	{ },
};

struct fdt_spm fdt_spm_mm = {
	.match_table = fdt_spm_mm_match,
	.setup = spm_mm_setup,
	.spm_message_handler = spm_message_handler_mm,
};

/* List of FDT SPM service group managers generated at compile time */
struct fdt_spm *fdt_spm_service_groups[] = { &fdt_spm_mm };
unsigned long fdt_spm_service_groups_size = sizeof(fdt_spm_service_groups) / sizeof(struct fdt_spm *);

int fdt_spm_request_manager(void *fdt, int nodeoff, struct fdt_spm **out_manager)
{
	int pos;
	struct fdt_spm *drv;

	for (pos = 0; pos < fdt_spm_service_groups_size; pos++) {
		drv = fdt_spm_service_groups[pos];

		if (fdt_match_node(fdt, nodeoff, drv->match_table)) {
			*out_manager = drv;
			return SBI_SUCCESS;
		}
	}

	/* Platforms have no message handler for this SPM instance */
	return SBI_EFAIL;
}

struct rpxy_spm_data {
	u32 service_group_id;
	int num_services;
	struct sbi_rpxy_service *services;
};

struct rpxy_spm {
	struct sbi_rpxy_service_group group;
	struct fdt_spm *manager;
};

static int rpxy_spm_send_message(struct sbi_rpxy_service_group *grp,
				  struct sbi_rpxy_service *srv,
				  void *tx, u32 tx_len,
				  void *rx, u32 rx_len,
				  unsigned long *ack_len)
{
	int ret;
	struct rpxy_spm *rspm = container_of(grp, struct rpxy_spm, group);

	ret = rspm->manager->spm_message_handler(srv->id, tx, tx_len, rx, rx_len, ack_len);

	return ret;
}

static int rpxy_spm_init(void *fdt, int nodeoff,
			  const struct fdt_match *match)
{
	int rc;
	struct rpxy_spm *rspm;
	struct fdt_spm *manager;
	const struct rpxy_spm_data *data = match->data;

	/* Allocate context for RPXY mbox client */
	rspm = sbi_zalloc(sizeof(*rspm));
	if (!rspm)
		return SBI_ENOMEM;

	/* Request SPM service group manager */
	rc = fdt_spm_request_manager(fdt, nodeoff, &manager);
	if (rc) {
		sbi_free(rspm);
		return 0;
	}

	/* Setup SPM service group manager, initialize SP context */
	rc = manager->setup(fdt, nodeoff, match);
	if (rc) {
		sbi_free(rspm);
		return 0;
	}

	/* Setup RPXY spm client */
	rspm->group.transport_id = 0;
	rspm->group.service_group_id = data->service_group_id;
	rspm->group.max_message_data_len = -1;
	rspm->group.num_services = data->num_services;
	rspm->group.services = data->services;
	rspm->group.send_message = rpxy_spm_send_message;
	rspm->manager = manager;

	/* Register RPXY service group */
	rc = sbi_rpxy_register_service_group(&rspm->group);
	if (rc) {
		sbi_free(rspm);
		return rc;
	}

	return 0;
}

static struct sbi_rpxy_service mm_services[] = {
{
	.id = RPMI_MM_SRV_MM_VERSION,
	.min_tx_len = 0,
	.max_tx_len = 0,
	.min_rx_len = sizeof(u32),
	.max_rx_len = sizeof(u32),
},
{
	.id = RPMI_MM_SRV_MM_COMMUNICATE,
	.min_tx_len = 0,
	.max_tx_len = 0x1000,
	.min_rx_len = 0,
	.max_rx_len = 0,
},
{
	.id = RPMI_MM_SRV_MM_COMPLETE,
	.min_tx_len = 0,
	.max_tx_len = 0x1000,
	.min_rx_len = 0,
	.max_rx_len = 0,
},
};

static struct rpxy_spm_data mm_data = {
	.service_group_id = RPMI_SRVGRP_SPM_MM,
	.num_services = array_size(mm_services),
	.services = mm_services,
};

static const struct fdt_match rpxy_spm_match[] = {
	{ .compatible = "riscv,rpmi-spm-mm", .data = &mm_data }, 
	{},
};

struct fdt_rpxy fdt_rpxy_spm = {
	.match_table = rpxy_spm_match,
	.init = rpxy_spm_init,
};
