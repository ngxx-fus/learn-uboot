// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2012 Stephen Warren
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <video.h>
#include <asm/arch/mbox.h>
#include <asm/arch/msg.h>
#include <asm/cache.h>

static int nf2711_video_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	int ret;
	int w, h, pitch;
	ulong fb_base, fb_size, fb_start, fb_end;

	debug("nf2711: Query resolution...\n");
	ret = nf2711_get_video_size(&w, &h);
	if (ret || w == 0 || h == 0)
		return -EIO;

	debug("nf2711: Setting up display for %d x %d\n", w, h);
	ret = nf2711_set_video_params(&w, &h, 32, BCM2835_MBOX_PIXEL_ORDER_RGB,
				       BCM2835_MBOX_ALPHA_MODE_IGNORED,
				       &fb_base, &fb_size, &pitch);
	if (ret)
		return -EIO;

	debug("nf2711: Final resolution is %d x %d\n", w, h);

	/* Enable dcache for the frame buffer */
	fb_start = fb_base & ~(MMU_SECTION_SIZE - 1);
	fb_end = fb_base + fb_size;
	fb_end = ALIGN(fb_end, 1 << MMU_SECTION_SHIFT);
	mmu_set_region_dcache_behaviour(fb_start, fb_end - fb_start,
					DCACHE_WRITEBACK);
	video_set_flush_dcache(dev, true);

	uc_priv->xsize = w;
	uc_priv->ysize = h;
	uc_priv->bpix = VIDEO_BPP32;
	plat->base = fb_base;
	plat->size = fb_size;

	return 0;
}

static const struct udevice_id nf2711_video_ids[] = {
	{ .compatible = "ngxxfus,nf2711-hdmi" },
	{ .compatible = "ngxxfus,nf2711-hdmi0" },
	{ .compatible = "ngxxfus,bcm2708-fb" },
	{ }
};

U_BOOT_DRIVER(nf2711_video) = {
	.name	= "nf2711_video",
	.id	= UCLASS_VIDEO,
	.of_match = nf2711_video_ids,
	.probe	= nf2711_video_probe,
};
