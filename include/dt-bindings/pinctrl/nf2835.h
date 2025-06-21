/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Header providing constants for nf2835 pinctrl bindings.
 *
 * Copyright (C) 2015 Stefan Wahren <stefan.wahren@i2se.com>
 */

#ifndef __DT_BINDINGS_PINCTRL_NF2835_H__
#define __DT_BINDINGS_PINCTRL_NF2835_H__

/* nfs,function property */
#define NF2835_FSEL_GPIO_IN	0
#define NF2835_FSEL_GPIO_OUT	1
#define NF2835_FSEL_ALT5	2
#define NF2835_FSEL_ALT4	3
#define NF2835_FSEL_ALT0	4
#define NF2835_FSEL_ALT1	5
#define NF2835_FSEL_ALT2	6
#define NF2835_FSEL_ALT3	7

/* nfs,pull property */
#define NF2835_PUD_OFF		0
#define NF2835_PUD_DOWN	1
#define NF2835_PUD_UP		2

#endif /* __DT_BINDINGS_PINCTRL_NF2835_H__ */
