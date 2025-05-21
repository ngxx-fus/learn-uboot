/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2012,2015 Stephen Warren
 */

#ifndef _NF2711_SDHCI_H_
#define _NF2711_SDHCI_H_

#include <asm/arch/base.h>

#define NF2711_SDHCI_PHYSADDR ({ BUG_ON(!rpi_nf271x_base); \
				  rpi_nf271x_base + 0x00300000; })

int nf2711_sdhci_init(u32 regbase, u32 emmc_freq);

#endif
