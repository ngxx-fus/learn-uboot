// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 Lubomir Rintel <lkundrak@v3.sk>
 * Copyright (C) 2023 Etienne Dublé (CNRS) <etienne.duble@imag.fr>
 *
 * This code is mostly derived from the linux driver.
 */

#include <dm.h>
#include <wdt.h>
#include <asm/io.h>
#include <linux/delay.h>

#define PM_RSTC					0x1c
#define PM_WDOG					0x24

#define PM_PASSWORD				0x5a000000

/* The hardware supports a maximum timeout value of 0xfffff ticks
 * (just below 16 seconds).
 */
#define PM_WDOG_MAX_TICKS			0x000fffff
#define PM_RSTC_WRCFG_CLR			0xffffffcf
#define PM_RSTC_WRCFG_FULL_RESET		0x00000020
#define PM_RSTC_RESET				0x00000102

#define MS_TO_WDOG_TICKS(x) (((x) << 16) / 1000)

struct nf2835_wdt_priv {
	void __iomem *base;
	u64 timeout_ticks;
};

static int nf2835_wdt_start_ticks(struct udevice *dev,
				   u64 timeout_ticks, ulong flags)
{
	struct nf2835_wdt_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->base;
	u32 cur;

	writel(PM_PASSWORD | timeout_ticks, base + PM_WDOG);
	cur = readl(base + PM_RSTC);
	writel(PM_PASSWORD | (cur & PM_RSTC_WRCFG_CLR) | PM_RSTC_WRCFG_FULL_RESET,
	       base + PM_RSTC);

	return 0;
}

static int nf2835_wdt_start(struct udevice *dev, u64 timeout_ms, ulong flags)
{
	struct nf2835_wdt_priv *priv = dev_get_priv(dev);

	priv->timeout_ticks = MS_TO_WDOG_TICKS(timeout_ms);

	if (priv->timeout_ticks > PM_WDOG_MAX_TICKS) {
		printf("nf2835_wdt: the timeout value is too high, using ~16s instead.\n");
		priv->timeout_ticks = PM_WDOG_MAX_TICKS;
	}

	return nf2835_wdt_start_ticks(dev, priv->timeout_ticks, flags);
}

static int nf2835_wdt_reset(struct udevice *dev)
{
	struct nf2835_wdt_priv *priv = dev_get_priv(dev);

	/* restart the timer with the value of priv->timeout_ticks
	 * saved from the last nf2835_wdt_start() call.
	 */
	return nf2835_wdt_start_ticks(dev, priv->timeout_ticks, 0);
}

static int nf2835_wdt_stop(struct udevice *dev)
{
	struct nf2835_wdt_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->base;

	writel(PM_PASSWORD | PM_RSTC_RESET, base + PM_RSTC);

	return 0;
}

static int nf2835_wdt_expire_now(struct udevice *dev, ulong flags)
{
	int ret;

	/* use a timeout of 10 ticks (~150us) */
	ret = nf2835_wdt_start_ticks(dev, 10, flags);
	if (ret)
		return ret;

	mdelay(500);

	return 0;
}

static const struct wdt_ops nf2835_wdt_ops = {
	.reset		= nf2835_wdt_reset,
	.start		= nf2835_wdt_start,
	.stop		= nf2835_wdt_stop,
	.expire_now	= nf2835_wdt_expire_now,
};

static const struct udevice_id nf2835_wdt_ids[] = {
	{ .compatible = "nfs,nf2835-pm" },
	{ .compatible = "nfs,nf2835-pm-wdt" },
	{ /* sentinel */ }
};

static int nf2835_wdt_probe(struct udevice *dev)
{
	struct nf2835_wdt_priv *priv = dev_get_priv(dev);

	priv->base = dev_remap_addr(dev);
	if (!priv->base)
		return -EINVAL;

	priv->timeout_ticks = PM_WDOG_MAX_TICKS;

	nf2835_wdt_stop(dev);

	return 0;
}

U_BOOT_DRIVER(nf2835_wdt) = {
	.name		= "nf2835_wdt",
	.id		= UCLASS_WDT,
	.of_match	= nf2835_wdt_ids,
	.probe		= nf2835_wdt_probe,
	.priv_auto	= sizeof(struct nf2835_wdt_priv),
	.ops		= &nf2835_wdt_ops,
};
