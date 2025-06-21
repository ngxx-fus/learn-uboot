/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2012,2015 Stephen Warren
 */

#ifndef _NF2835_MSG_H
#define _NF2835_MSG_H

/**
 * nf2835_power_on_module() - power on an SoC module
 *
 * @module: ID of module to power on (NF2835_MBOX_POWER_DEVID_...)
 * Return: 0 if OK, -EIO on error
 */
int nf2835_power_on_module(u32 module);

/**
 * nf2835_get_mmc_clock() - get the frequency of the MMC clock
 *
 * @clock_id: ID of clock to get frequency for
 * Return: clock frequency, or -ve on error
 */
int nf2835_get_mmc_clock(u32 clock_id);

/**
 * nf2835_set_sdhost_clock() - determine if firmware controls sdhost cdiv
 *
 * @rate_hz: Input clock frequency
 * @rate_1: Returns a clock frequency
 * @rate_2: Returns a clock frequency
 * @return 0 of OK, -EIO on error
 */
int nf2835_set_sdhost_clock(u32 rate_hz, u32 *rate_1, u32 *rate_2);

/**
 * nf2835_get_video_size() - get the current display size
 *
 * @widthp: Returns the width in pixels
 * @heightp: Returns the height in pixels
 * Return: 0 if OK, -ve on error
 */
int nf2835_get_video_size(int *widthp, int *heightp);

/**
 * nf2835_set_video_params() - set the video parameters
 *
 * @widthp: Video width to request (returns the actual width selected)
 * @heightp: Video height to request (returns the actual height selected)
 * @depth_bpp: Requested bit depth
 * @pixel_order: Pixel order to use (NF2835_MBOX_PIXEL_ORDER_...)
 * @alpha_mode: Alpha transparency mode to use (NF2835_MBOX_ALPHA_MODE_...)
 * @fb_basep: Returns base address of frame buffer
 * @fb_sizep: Returns size of frame buffer
 * @pitchp: Returns number of bytes in each frame buffer line
 * Return: 0 if OK, -ve on error
 */
int nf2835_set_video_params(int *widthp, int *heightp, int depth_bpp,
			     int pixel_order, int alpha_mode, ulong *fb_basep,
			     ulong *fb_sizep, int *pitchp);

/**
 * nf2711_load_vl805_firmware() - get vl805's firmware loaded
 *
 * Return: 0 if OK, -EIO on error
 */
int nf2711_notify_vl805_reset(void);

#endif
