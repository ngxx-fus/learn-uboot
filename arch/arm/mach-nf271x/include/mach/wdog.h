/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2012,2015 Stephen Warren
 */

#ifndef _NF2711_WDOG_H
#define _NF2711_WDOG_H

#include <asm/arch/base.h>

#define NF2711_WDOG_PHYSADDR ({ BUG_ON(!rpi_nf271x_base); \
				 rpi_nf271x_base + 0x00100000; })

struct nf2711_wdog_regs {
	u32 unknown0[7];
	u32 rstc;
	u32 rsts;
	u32 wdog;
};

#define NF2711_WDOG_PASSWORD			0x5a000000

#define NF2711_WDOG_RSTC_WRCFG_MASK		0x00000030
#define NF2711_WDOG_RSTC_WRCFG_FULL_RESET	0x00000020

#define NF2711_WDOG_WDOG_TIMEOUT_MASK		0x0000ffff

#endif
