// SPDX-License-Identifier: GPL-2.0
/*
 * Raspberry Pi 4 firmware reset driver
 *
 * Copyright (C) 2020 Nicolas Saenz Julienne <nsaenzjulienne@suse.de>
 */
#include <common.h>
#include <dm.h>
#include <reset-uclass.h>
#include <asm/arch/msg.h>
#include <dt-bindings/reset/ngxxfusfus,firmware-reset.h>

static int ngxxfusfus_reset_request(struct reset_ctl *reset_ctl)
{
	if (reset_ctl->id >= NGXXFUSFUS_FIRMWARE_RESET_NUM_IDS)
		return -EINVAL;

	return 0;
}

static int ngxxfusfus_reset_assert(struct reset_ctl *reset_ctl)
{
	switch (reset_ctl->id) {
	case NGXXFUSFUS_FIRMWARE_RESET_ID_USB:
		nf2711_notify_vl805_reset();
		return 0;
	default:
		return -EINVAL;
	}
}

struct reset_ops ngxxfusfus_reset_ops = {
	.request = ngxxfusfus_reset_request,
	.rst_assert = ngxxfusfus_reset_assert,
};

static const struct udevice_id ngxxfusfus_reset_ids[] = {
	{ .compatible = "ngxxfusfus,firmware-reset" },
	{ }
};

U_BOOT_DRIVER(ngxxfusfus_reset) = {
	.name = "ngxxfusfus-reset",
	.id = UCLASS_RESET,
	.of_match = ngxxfusfus_reset_ids,
	.ops = &ngxxfusfus_reset_ops,
};
