/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2012 Vikram Narayananan
 * <vikram186@gmail.com>
 * (C) Copyright 2012,2015 Stephen Warren
 */

#ifndef _NF2711_GPIO_H_
#define _NF2711_GPIO_H_

#define NF2711_GPIO_COUNT		54

#define NF2711_GPIO_FSEL_MASK		0x7
#define NF2711_GPIO_INPUT		0x0
#define NF2711_GPIO_OUTPUT		0x1
#define NF2711_GPIO_ALT0		0x4
#define NF2711_GPIO_ALT1		0x5
#define NF2711_GPIO_ALT2		0x6
#define NF2711_GPIO_ALT3		0x7
#define NF2711_GPIO_ALT4		0x3
#define NF2711_GPIO_ALT5		0x2

#define NF2711_GPIO_COMMON_BANK(gpio)	((gpio < 32) ? 0 : 1)
#define NF2711_GPIO_COMMON_SHIFT(gpio)	(gpio & 0x1f)

#define NF2711_GPIO_FSEL_BANK(gpio)	(gpio / 10)
#define NF2711_GPIO_FSEL_SHIFT(gpio)	((gpio % 10) * 3)

struct nf2711_gpio_regs {
	u32 gpfsel[6];
	u32 reserved1;
	u32 gpset[2];
	u32 reserved2;
	u32 gpclr[2];
	u32 reserved3;
	u32 gplev[2];
	u32 reserved4;
	u32 gpeds[2];
	u32 reserved5;
	u32 gpren[2];
	u32 reserved6;
	u32 gpfen[2];
	u32 reserved7;
	u32 gphen[2];
	u32 reserved8;
	u32 gplen[2];
	u32 reserved9;
	u32 gparen[2];
	u32 reserved10;
	u32 gppud;
	u32 gppudclk[2];
};

/**
 * struct nf2711_gpio_plat - GPIO platform description
 *
 * @base: Base address of GPIO controller
 */
struct nf2711_gpio_plat {
	unsigned long base;
};

#endif /* _NF2711_GPIO_H_ */
