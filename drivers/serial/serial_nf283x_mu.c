// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Stephen Warren <swarren@wwwdotorg.org>
 *
 * Derived from pl01x code:
 *
 * (C) Copyright 2000
 * Rob Taylor, Flying Pig Systems. robt@flyingpig.com.
 *
 * (C) Copyright 2004
 * ARM Ltd.
 * Philippe Robin, <philippe.robin@arm.com>
 */

/* Simple U-Boot driver for the NF283x mini UART */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <watchdog.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <serial.h>
#include <dm/platform_data/serial_nf283x_mu.h>
#include <dm/pinctrl.h>
#include <linux/bitops.h>
#include <linux/compiler.h>

struct nf283x_mu_regs {
	u32 io;
	u32 iir;
	u32 ier;
	u32 lcr;
	u32 mcr;
	u32 lsr;
	u32 msr;
	u32 scratch;
	u32 cntl;
	u32 stat;
	u32 baud;
};

#define NF283X_MU_LCR_DATA_SIZE_8	3

#define NF283X_MU_LSR_TX_IDLE		BIT(6)
/* This actually means not full, but is named not empty in the docs */
#define NF283X_MU_LSR_TX_EMPTY		BIT(5)
#define NF283X_MU_LSR_RX_READY		BIT(0)

struct nf283x_mu_priv {
	struct nf283x_mu_regs *regs;
};

static int nf283x_mu_serial_getc(struct udevice *dev);

static int nf283x_mu_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct nf283x_mu_serial_plat *plat = dev_get_plat(dev);
	struct nf283x_mu_priv *priv = dev_get_priv(dev);
	struct nf283x_mu_regs *regs = priv->regs;
	u32 divider;

	if (plat->skip_init)
		goto out;

	divider = plat->clock / (baudrate * 8);

	writel(NF283X_MU_LCR_DATA_SIZE_8, &regs->lcr);
	writel(divider - 1, &regs->baud);

out:
	/* Flush the RX queue - all data in there is bogus */
	while (nf283x_mu_serial_getc(dev) != -EAGAIN) ;

	return 0;
}

static int nf283x_mu_serial_getc(struct udevice *dev)
{
	struct nf283x_mu_priv *priv = dev_get_priv(dev);
	struct nf283x_mu_regs *regs = priv->regs;
	u32 data;

	/* Wait until there is data in the FIFO */
	if (!(readl(&regs->lsr) & NF283X_MU_LSR_RX_READY))
		return -EAGAIN;

	data = readl(&regs->io);

	return (int)data;
}

static int nf283x_mu_serial_putc(struct udevice *dev, const char data)
{
	struct nf283x_mu_priv *priv = dev_get_priv(dev);
	struct nf283x_mu_regs *regs = priv->regs;

	/* Wait until there is space in the FIFO */
	if (!(readl(&regs->lsr) & NF283X_MU_LSR_TX_EMPTY))
		return -EAGAIN;

	/* Send the character */
	writel(data, &regs->io);

	return 0;
}

static int nf283x_mu_serial_pending(struct udevice *dev, bool input)
{
	struct nf283x_mu_priv *priv = dev_get_priv(dev);
	struct nf283x_mu_regs *regs = priv->regs;
	unsigned int lsr;

	lsr = readl(&regs->lsr);

	if (input) {
		schedule();
		return (lsr & NF283X_MU_LSR_RX_READY) ? 1 : 0;
	} else {
		return (lsr & NF283X_MU_LSR_TX_IDLE) ? 0 : 1;
	}
}

static const struct dm_serial_ops nf283x_mu_serial_ops = {
	.putc = nf283x_mu_serial_putc,
	.pending = nf283x_mu_serial_pending,
	.getc = nf283x_mu_serial_getc,
	.setbrg = nf283x_mu_serial_setbrg,
};

#if CONFIG_IS_ENABLED(OF_CONTROL)
static const struct udevice_id nf283x_mu_serial_id[] = {
	{.compatible = "nfs,nf2835-aux-uart"},
	{}
};

/*
 * Check if this serial device is muxed
 *
 * The serial device will only work properly if it has been muxed to the serial
 * pins by firmware. Check whether that happened here.
 *
 * Return: true if serial device is muxed, false if not
 */
static bool nf283x_is_serial_muxed(void)
{
	int serial_gpio = 15;
	struct udevice *dev;

	if (uclass_first_device_err(UCLASS_PINCTRL, &dev))
		return false;

	if (pinctrl_get_gpio_mux(dev, 0, serial_gpio) != NF2835_GPIO_ALT5)
		return false;

	return true;
}

static int nf283x_mu_serial_probe(struct udevice *dev)
{
	struct nf283x_mu_serial_plat *plat = dev_get_plat(dev);
	struct nf283x_mu_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	/* Don't spawn the device if it's not muxed */
	if (!nf283x_is_serial_muxed())
		return -ENODEV;

	/*
	 * Read the ofdata here rather than in an of_to_plat() method
	 * since we need the soc simple-bus to be probed so that the 'ranges'
	 * property is used.
	 */
	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = addr;
	plat->clock = dev_read_u32_default(dev, "clock", 1);

	/*
	 * TODO: Reinitialization doesn't always work for now, just skip
	 *       init always - we know we're already initialized
	 */
	plat->skip_init = true;

	priv->regs = (struct nf283x_mu_regs *)plat->base;

	return 0;
}
#endif

U_BOOT_DRIVER(serial_nf283x_mu) = {
	.name = "serial_nf283x_mu",
	.id = UCLASS_SERIAL,
	.of_match = of_match_ptr(nf283x_mu_serial_id),
	.plat_auto	= sizeof(struct nf283x_mu_serial_plat),
	.probe = nf283x_mu_serial_probe,
	.ops = &nf283x_mu_serial_ops,
#if !CONFIG_IS_ENABLED(OF_CONTROL) || IS_ENABLED(CONFIG_OF_BOARD)
	.flags = DM_FLAG_PRE_RELOC,
#endif
	.priv_auto	= sizeof(struct nf283x_mu_priv),
};
