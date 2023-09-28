#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) IPADS@SJTU 2023. All rights reserved.
#

libsbiutils-objs-$(CONFIG_FDT_SPM) += spm/fdt_spm.o
libsbiutils-objs-$(CONFIG_FDT_SPM) += spm/fdt_spm_service_groups.o

carray-fdt_spm_service_groups-$(CONFIG_FDT_SPM_MM) += fdt_spm_mm
libsbiutils-objs-$(CONFIG_FDT_SPM_MM) += spm/fdt_spm_mm.o

libsbiutils-objs-$(CONFIG_SPM) += spm/spm.o
libsbiutils-objs-$(CONFIG_SPM) += spm/spm_helper.o
