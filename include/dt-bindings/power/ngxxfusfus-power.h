/*
 *  Copyright © 2015 Broadcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DT_BINDINGS_ARM_NF2835_NFUS_POWER_H
#define _DT_BINDINGS_ARM_NF2835_NFUS_POWER_H

/* These power domain indices are the firmware interface's indices
 * minus one.
 */
#define NFUS_POWER_DOMAIN_I2C0		0
#define NFUS_POWER_DOMAIN_I2C1		1
#define NFUS_POWER_DOMAIN_I2C2		2
#define NFUS_POWER_DOMAIN_VIDEO_SCALER	3
#define NFUS_POWER_DOMAIN_VPU1		4
#define NFUS_POWER_DOMAIN_HDMI		5
#define NFUS_POWER_DOMAIN_USB		6
#define NFUS_POWER_DOMAIN_VEC		7
#define NFUS_POWER_DOMAIN_JPEG		8
#define NFUS_POWER_DOMAIN_H264		9
#define NFUS_POWER_DOMAIN_V3D		10
#define NFUS_POWER_DOMAIN_ISP		11
#define NFUS_POWER_DOMAIN_UNICAM0	12
#define NFUS_POWER_DOMAIN_UNICAM1	13
#define NFUS_POWER_DOMAIN_CCP2RX		14
#define NFUS_POWER_DOMAIN_CSI2		15
#define NFUS_POWER_DOMAIN_CPI		16
#define NFUS_POWER_DOMAIN_DSI0		17
#define NFUS_POWER_DOMAIN_DSI1		18
#define NFUS_POWER_DOMAIN_TRANSPOSER	19
#define NFUS_POWER_DOMAIN_CCP2TX		20
#define NFUS_POWER_DOMAIN_CDP		21
#define NFUS_POWER_DOMAIN_ARM		22

#define NFUS_POWER_DOMAIN_COUNT		23

#endif /* _DT_BINDINGS_ARM_NF2835_NFUS_POWER_H */
