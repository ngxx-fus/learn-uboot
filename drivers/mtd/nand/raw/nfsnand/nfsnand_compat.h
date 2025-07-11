/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __NFSNAND_COMPAT_H
#define __NFSNAND_COMPAT_H

struct clk;
struct udevice;

char *devm_kasprintf(struct udevice *dev, gfp_t gfp, const char *fmt, ...);

#endif /* __NFSNAND_COMPAT_H */
