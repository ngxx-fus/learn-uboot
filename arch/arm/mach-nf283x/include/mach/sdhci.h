/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2012,2015 Stephen Warren
 */

#ifndef _NF2835_SDHCI_H_
#define _NF2835_SDHCI_H_

#include <asm/arch/base.h>

#define NF2835_SDHCI_PHYSADDR ({ BUG_ON(!nfus_nf283x_base); \
				  nfus_nf283x_base + 0x00300000; })

int nf2835_sdhci_init(u32 regbase, u32 emmc_freq);

#endif
