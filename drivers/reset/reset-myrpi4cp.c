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
#include <dt-bindings/reset/ngxxfus,firmware-reset.h>

static int ngxxfus_reset_request(struct reset_ctl *reset_ctl)
{
	if (reset_ctl->id >= NGXXFUS_FIRMWARE_RESET_NUM_IDS)
		return -EINVAL;

	return 0;
}

static int ngxxfus_reset_assert(struct reset_ctl *reset_ctl)
{
	switch (reset_ctl->id) {
	case NGXXFUS_FIRMWARE_RESET_ID_USB:
		nf2711_notify_vl805_reset();
		return 0;
	default:
		return -EINVAL;
	}
}

struct reset_ops ngxxfus_reset_ops = {
	.request = ngxxfus_reset_request,
	.rst_assert = ngxxfus_reset_assert,
};

static const struct udevice_id ngxxfus_reset_ids[] = {
	{ .compatible = "ngxxfus,firmware-reset" },
	{ }
};

U_BOOT_DRIVER(ngxxfus_reset) = {
	.name = "ngxxfus-reset",
	.id = UCLASS_RESET,
	.of_match = ngxxfus_reset_ids,
	.ops = &ngxxfus_reset_ops,
};
