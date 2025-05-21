/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Header providing constants for bcm2835 pinctrl bindings.
 *
 * Copyright (C) 2015 Stefan Wahren <stefan.wahren@i2se.com>
 */

#ifndef __DT_BINDINGS_PINCTRL_NF2711_H__
#define __DT_BINDINGS_PINCTRL_NF2711_H__

/* brcm,function property */
#define NF2711_FSEL_GPIO_IN	0
#define NF2711_FSEL_GPIO_OUT	1
#define NF2711_FSEL_ALT5	2
#define NF2711_FSEL_ALT4	3
#define NF2711_FSEL_ALT0	4
#define NF2711_FSEL_ALT1	5
#define NF2711_FSEL_ALT2	6
#define NF2711_FSEL_ALT3	7

/* brcm,pull property */
#define NF2711_PUD_OFF		0
#define NF2711_PUD_DOWN	1
#define NF2711_PUD_UP		2

#endif /* __DT_BINDINGS_PINCTRL_NF2711_H__ */
