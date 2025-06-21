// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2012 Vikram Narayananan
 * <vikram186@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <errno.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <fdtdec.h>

struct nf2835_gpios {
	struct nf2835_gpio_regs *reg;
	struct udevice *pinctrl;
};

static int nf2835_gpio_direction_input(struct udevice *dev, unsigned gpio)
{
	struct nf2835_gpios *gpios = dev_get_priv(dev);
	unsigned val;

	val = readl(&gpios->reg->gpfsel[NF2835_GPIO_FSEL_BANK(gpio)]);
	val &= ~(NF2835_GPIO_FSEL_MASK << NF2835_GPIO_FSEL_SHIFT(gpio));
	val |= (NF2835_GPIO_INPUT << NF2835_GPIO_FSEL_SHIFT(gpio));
	writel(val, &gpios->reg->gpfsel[NF2835_GPIO_FSEL_BANK(gpio)]);

	return 0;
}

static int nf2835_gpio_direction_output(struct udevice *dev, unsigned int gpio,
					 int value)
{
	struct nf2835_gpios *gpios = dev_get_priv(dev);
	unsigned val;

	gpio_set_value(gpio, value);

	val = readl(&gpios->reg->gpfsel[NF2835_GPIO_FSEL_BANK(gpio)]);
	val &= ~(NF2835_GPIO_FSEL_MASK << NF2835_GPIO_FSEL_SHIFT(gpio));
	val |= (NF2835_GPIO_OUTPUT << NF2835_GPIO_FSEL_SHIFT(gpio));
	writel(val, &gpios->reg->gpfsel[NF2835_GPIO_FSEL_BANK(gpio)]);

	return 0;
}

static int nf2835_get_value(const struct nf2835_gpios *gpios, unsigned gpio)
{
	unsigned val;

	val = readl(&gpios->reg->gplev[NF2835_GPIO_COMMON_BANK(gpio)]);

	return (val >> NF2835_GPIO_COMMON_SHIFT(gpio)) & 0x1;
}

static int nf2835_gpio_get_value(struct udevice *dev, unsigned gpio)
{
	const struct nf2835_gpios *gpios = dev_get_priv(dev);

	return nf2835_get_value(gpios, gpio);
}

static int nf2835_gpio_set_value(struct udevice *dev, unsigned gpio,
				  int value)
{
	struct nf2835_gpios *gpios = dev_get_priv(dev);
	u32 *output_reg = value ? gpios->reg->gpset : gpios->reg->gpclr;

	writel(1 << NF2835_GPIO_COMMON_SHIFT(gpio),
				&output_reg[NF2835_GPIO_COMMON_BANK(gpio)]);

	return 0;
}

static int nf2835_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct nf2835_gpios *priv = dev_get_priv(dev);
	int funcid;

	funcid = pinctrl_get_gpio_mux(priv->pinctrl, 0, offset);

	switch (funcid) {
	case NF2835_GPIO_OUTPUT:
		return GPIOF_OUTPUT;
	case NF2835_GPIO_INPUT:
		return GPIOF_INPUT;
	default:
		return GPIOF_FUNC;
	}
}

static const struct dm_gpio_ops gpio_nf2835_ops = {
	.direction_input	= nf2835_gpio_direction_input,
	.direction_output	= nf2835_gpio_direction_output,
	.get_value		= nf2835_gpio_get_value,
	.set_value		= nf2835_gpio_set_value,
	.get_function		= nf2835_gpio_get_function,
};

static int nf2835_gpio_probe(struct udevice *dev)
{
	struct nf2835_gpios *gpios = dev_get_priv(dev);
	struct nf2835_gpio_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->bank_name = "GPIO";
	uc_priv->gpio_count = NF2835_GPIO_COUNT;
	gpios->reg = (struct nf2835_gpio_regs *)plat->base;

	/* We know we're spawned by the pinctrl driver */
	gpios->pinctrl = dev->parent;

	return 0;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int nf2835_gpio_of_to_plat(struct udevice *dev)
{
	struct nf2835_gpio_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = addr;
	return 0;
}
#endif

U_BOOT_DRIVER(gpio_nf2835) = {
	.name	= "gpio_nf2835",
	.id	= UCLASS_GPIO,
	.of_to_plat = of_match_ptr(nf2835_gpio_of_to_plat),
	.plat_auto	= sizeof(struct nf2835_gpio_plat),
	.ops	= &gpio_nf2835_ops,
	.probe	= nf2835_gpio_probe,
	.flags	= DM_FLAG_PRE_RELOC,
	.priv_auto	= sizeof(struct nf2835_gpios),
};
