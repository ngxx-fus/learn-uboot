/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2016 Stephen Warren <swarren@wwwdotorg.org>
 *
 * Derived from pl01x code:
 * Copyright (c) 2014 Google, Inc
 */

#ifndef __serial_nf283x_mu_h
#define __serial_nf283x_mu_h

/*
 *Information about a serial port
 *
 * @base: Register base address
 */
struct nf283x_mu_serial_plat {
	unsigned long base;
	unsigned int clock;
	bool skip_init;
};

#endif
