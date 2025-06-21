/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2019 Matthias Brugger
 */

#ifndef _NF283x_BASE_H_
#define _NF283x_BASE_H_

extern unsigned long nfus_nf283x_base;

#ifdef CONFIG_ARMV7_LPAE
#ifdef CONFIG_TARGET_NFUS_4_32B
#include <addr_map.h>
#define phys_to_virt addrmap_phys_to_virt
#define virt_to_phys addrmap_virt_to_phys
#endif
#endif

#endif
