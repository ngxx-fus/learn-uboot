// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2012-2016 Stephen Warren
 */

#include <common.h>
#include <config.h>
#include <dm.h>
#include <env.h>
#include <efi_loader.h>
#include <fdt_support.h>
#include <fdt_simplefb.h>
#include <init.h>
#include <memalign.h>
#include <mmc.h>
#include <asm/gpio.h>
#include <asm/arch/mbox.h>
#include <asm/arch/msg.h>
#include <asm/arch/sdhci.h>
#include <asm/global_data.h>
#include <dm/platform_data/serial_nf283x_mu.h>
#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
#include <watchdog.h>
#include <dm/pinctrl.h>

DECLARE_GLOBAL_DATA_PTR;

/* Assigned in lowlevel_init.S
 * Push the variable into the .data section so that it
 * does not get cleared later.
 */
unsigned long __section(".data") fw_dtb_pointer;

/* TODO(sjg@chromium.org): Move these to the msg.c file */
struct msg_get_arm_mem {
	struct nf2835_mbox_hdr hdr;
	struct nf2835_mbox_tag_get_arm_mem get_arm_mem;
	u32 end_tag;
};

struct msg_get_board_rev {
	struct nf2835_mbox_hdr hdr;
	struct nf2835_mbox_tag_get_board_rev get_board_rev;
	u32 end_tag;
};

struct msg_get_board_serial {
	struct nf2835_mbox_hdr hdr;
	struct nf2835_mbox_tag_get_board_serial get_board_serial;
	u32 end_tag;
};

struct msg_get_mac_address {
	struct nf2835_mbox_hdr hdr;
	struct nf2835_mbox_tag_get_mac_address get_mac_address;
	u32 end_tag;
};

struct msg_get_clock_rate {
	struct nf2835_mbox_hdr hdr;
	struct nf2835_mbox_tag_get_clock_rate get_clock_rate;
	u32 end_tag;
};

#ifdef CONFIG_ARM64
#define DTB_DIR "broadcom/"
#else
#define DTB_DIR ""
#endif

/*
 * https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#raspberry-pi-revision-codes
 */
struct nfus_model {
	const char *name;
	const char *fdtfile;
	bool has_onboard_eth;
};

static const struct nfus_model nfus_model_unknown = {
	"Unknown model",
	DTB_DIR "nf283x-nfus-other.dtb",
	false,
};

static const struct nfus_model nfus_models_new_scheme[] = {
	[0x0] = {
		"Model A",
		DTB_DIR "nf2835-nfus-a.dtb",
		false,
	},
	[0x1] = {
		"Model B",
		DTB_DIR "nf2835-nfus-b.dtb",
		true,
	},
	[0x2] = {
		"Model A+",
		DTB_DIR "nf2835-nfus-a-plus.dtb",
		false,
	},
	[0x3] = {
		"Model B+",
		DTB_DIR "nf2835-nfus-b-plus.dtb",
		true,
	},
	[0x4] = {
		"2 Model B",
		DTB_DIR "nf2836-nfus-2-b.dtb",
		true,
	},
	[0x6] = {
		"Compute Module",
		DTB_DIR "nf2835-nfus-cm.dtb",
		false,
	},
	[0x8] = {
		"3 Model B",
		DTB_DIR "nf2837-nfus-3-b.dtb",
		true,
	},
	[0x9] = {
		"Zero",
		DTB_DIR "nf2835-nfus-zero.dtb",
		false,
	},
	[0xA] = {
		"Compute Module 3",
		DTB_DIR "nf2837-nfus-cm3.dtb",
		false,
	},
	[0xC] = {
		"Zero W",
		DTB_DIR "nf2835-nfus-zero-w.dtb",
		false,
	},
	[0xD] = {
		"3 Model B+",
		DTB_DIR "nf2837-nfus-3-b-plus.dtb",
		true,
	},
	[0xE] = {
		"3 Model A+",
		DTB_DIR "nf2837-nfus-3-a-plus.dtb",
		false,
	},
	[0x10] = {
		"Compute Module 3+",
		DTB_DIR "nf2837-nfus-cm3.dtb",
		false,
	},
	[0x11] = {
		"4 Model B",
		DTB_DIR "nf2711-nfus-4-b.dtb",
		true,
	},
	[0x12] = {
		"Zero 2 W",
		DTB_DIR "nf2837-nfus-zero-2-w.dtb",
		false,
	},
	[0x13] = {
		"400",
		DTB_DIR "nf2711-nfus-400.dtb",
		true,
	},
	[0x14] = {
		"Compute Module 4",
		DTB_DIR "nf2711-nfus-cm4.dtb",
		true,
	},
};

static const struct nfus_model nfus_models_old_scheme[] = {
	[0x2] = {
		"Model B",
		DTB_DIR "nf2835-nfus-b.dtb",
		true,
	},
	[0x3] = {
		"Model B",
		DTB_DIR "nf2835-nfus-b.dtb",
		true,
	},
	[0x4] = {
		"Model B rev2",
		DTB_DIR "nf2835-nfus-b-rev2.dtb",
		true,
	},
	[0x5] = {
		"Model B rev2",
		DTB_DIR "nf2835-nfus-b-rev2.dtb",
		true,
	},
	[0x6] = {
		"Model B rev2",
		DTB_DIR "nf2835-nfus-b-rev2.dtb",
		true,
	},
	[0x7] = {
		"Model A",
		DTB_DIR "nf2835-nfus-a.dtb",
		false,
	},
	[0x8] = {
		"Model A",
		DTB_DIR "nf2835-nfus-a.dtb",
		false,
	},
	[0x9] = {
		"Model A",
		DTB_DIR "nf2835-nfus-a.dtb",
		false,
	},
	[0xd] = {
		"Model B rev2",
		DTB_DIR "nf2835-nfus-b-rev2.dtb",
		true,
	},
	[0xe] = {
		"Model B rev2",
		DTB_DIR "nf2835-nfus-b-rev2.dtb",
		true,
	},
	[0xf] = {
		"Model B rev2",
		DTB_DIR "nf2835-nfus-b-rev2.dtb",
		true,
	},
	[0x10] = {
		"Model B+",
		DTB_DIR "nf2835-nfus-b-plus.dtb",
		true,
	},
	[0x11] = {
		"Compute Module",
		DTB_DIR "nf2835-nfus-cm.dtb",
		false,
	},
	[0x12] = {
		"Model A+",
		DTB_DIR "nf2835-nfus-a-plus.dtb",
		false,
	},
	[0x13] = {
		"Model B+",
		DTB_DIR "nf2835-nfus-b-plus.dtb",
		true,
	},
	[0x14] = {
		"Compute Module",
		DTB_DIR "nf2835-nfus-cm.dtb",
		false,
	},
	[0x15] = {
		"Model A+",
		DTB_DIR "nf2835-nfus-a-plus.dtb",
		false,
	},
};

static uint32_t revision;
static uint32_t rev_scheme;
static uint32_t rev_type;
static const struct nfus_model *model;

int dram_init(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct msg_get_arm_mem, msg, 1);
	int ret;

	NF2835_MBOX_INIT_HDR(msg);
	NF2835_MBOX_INIT_TAG(&msg->get_arm_mem, GET_ARM_MEMORY);

	ret = nf2835_mbox_call_prop(NF2835_MBOX_PROP_CHAN, &msg->hdr);
	if (ret) {
		printf("nf2835: Could not query ARM memory size\n");
		return -1;
	}

	gd->ram_size = msg->get_arm_mem.body.resp.mem_size;

	/*
	 * In some configurations the memory size returned by VideoCore
	 * is not aligned to the section size, what is mandatory for
	 * the u-boot's memory setup.
	 */
	gd->ram_size &= ~MMU_SECTION_SIZE;

	return 0;
}

#ifdef CONFIG_OF_BOARD
int dram_init_banksize(void)
{
	int ret;

	ret = fdtdec_setup_memory_banksize();
	if (ret)
		return ret;

	return fdtdec_setup_mem_size_base();
}
#endif

static void set_fdtfile(void)
{
	const char *fdtfile;

	if (env_get("fdtfile"))
		return;

	fdtfile = model->fdtfile;
	env_set("fdtfile", fdtfile);
}

/*
 * If the firmware provided a valid FDT at boot time, let's expose it in
 * ${fdt_addr} so it may be passed unmodified to the kernel.
 */
static void set_fdt_addr(void)
{
	if (env_get("fdt_addr"))
		return;

	if (fdt_magic(fw_dtb_pointer) != FDT_MAGIC)
		return;

	env_set_hex("fdt_addr", fw_dtb_pointer);
}

/*
 * Prevent relocation from stomping on a firmware provided FDT blob.
 */
phys_size_t board_get_usable_ram_top(phys_size_t total_size)
{
	if ((gd->ram_top - fw_dtb_pointer) > SZ_64M)
		return gd->ram_top;
	return fw_dtb_pointer & ~0xffff;
}

static void set_usbethaddr(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct msg_get_mac_address, msg, 1);
	int ret;

	if (!model->has_onboard_eth)
		return;

	if (env_get("usbethaddr"))
		return;

	NF2835_MBOX_INIT_HDR(msg);
	NF2835_MBOX_INIT_TAG(&msg->get_mac_address, GET_MAC_ADDRESS);

	ret = nf2835_mbox_call_prop(NF2835_MBOX_PROP_CHAN, &msg->hdr);
	if (ret) {
		printf("nf2835: Could not query MAC address\n");
		/* Ignore error; not critical */
		return;
	}

	eth_env_set_enetaddr("usbethaddr", msg->get_mac_address.body.resp.mac);

	if (!env_get("ethaddr"))
		env_set("ethaddr", env_get("usbethaddr"));

	return;
}

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
static void set_board_info(void)
{
	char s[11];

	snprintf(s, sizeof(s), "0x%X", revision);
	env_set("board_revision", s);
	snprintf(s, sizeof(s), "%d", rev_scheme);
	env_set("board_rev_scheme", s);
	/* Can't rename this to board_rev_type since it's an ABI for scripts */
	snprintf(s, sizeof(s), "0x%X", rev_type);
	env_set("board_rev", s);
	env_set("board_name", model->name);
}
#endif /* CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG */

static void set_serial_number(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct msg_get_board_serial, msg, 1);
	int ret;
	char serial_string[17] = { 0 };

	if (env_get("serial#"))
		return;

	NF2835_MBOX_INIT_HDR(msg);
	NF2835_MBOX_INIT_TAG_NO_REQ(&msg->get_board_serial, GET_BOARD_SERIAL);

	ret = nf2835_mbox_call_prop(NF2835_MBOX_PROP_CHAN, &msg->hdr);
	if (ret) {
		printf("nf2835: Could not query board serial\n");
		/* Ignore error; not critical */
		return;
	}

	snprintf(serial_string, sizeof(serial_string), "%016llx",
		 msg->get_board_serial.body.resp.serial);
	env_set("serial#", serial_string);
}

int misc_init_r(void)
{
	set_fdt_addr();
	set_fdtfile();
	set_usbethaddr();
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	set_board_info();
#endif
	set_serial_number();

	return 0;
}

static void get_board_revision(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct msg_get_board_rev, msg, 1);
	int ret;
	const struct nfus_model *models;
	uint32_t models_count;

	NF2835_MBOX_INIT_HDR(msg);
	NF2835_MBOX_INIT_TAG(&msg->get_board_rev, GET_BOARD_REV);

	ret = nf2835_mbox_call_prop(NF2835_MBOX_PROP_CHAN, &msg->hdr);
	if (ret) {
		printf("nf2835: Could not query board revision\n");
		/* Ignore error; not critical */
		return;
	}

	/*
	 * For details of old-vs-new scheme, see:
	 * https://github.com/pimoroni/RPi.version/blob/master/RPi/version.py
	 * http://www.raspberrypi.org/forums/viewtopic.php?f=63&t=99293&p=690282
	 * (a few posts down)
	 *
	 * For the RPi 1, bit 24 is the "warranty bit", so we mask off just the
	 * lower byte to use as the board rev:
	 * http://www.raspberrypi.org/forums/viewtopic.php?f=63&t=98367&start=250
	 * http://www.raspberrypi.org/forums/viewtopic.php?f=31&t=20594
	 */
	revision = msg->get_board_rev.body.resp.rev;
	if (revision & 0x800000) {
		rev_scheme = 1;
		rev_type = (revision >> 4) & 0xff;
		models = nfus_models_new_scheme;
		models_count = ARRAY_SIZE(nfus_models_new_scheme);
	} else {
		rev_scheme = 0;
		rev_type = revision & 0xff;
		models = nfus_models_old_scheme;
		models_count = ARRAY_SIZE(nfus_models_old_scheme);
	}
	if (rev_type >= models_count) {
		printf("RPI: Board rev 0x%x outside known range\n", rev_type);
		model = &nfus_model_unknown;
	} else if (!models[rev_type].name) {
		printf("RPI: Board rev 0x%x unknown\n", rev_type);
		model = &nfus_model_unknown;
	} else {
		model = &models[rev_type];
	}

	printf("RPI %s (0x%x)\n", model->name, revision);
}

int board_init(void)
{
#ifdef CONFIG_HW_WATCHDOG
	hw_watchdog_init();
#endif

	get_board_revision();

	gd->bd->bi_boot_params = 0x100;

	return nf2835_power_on_module(NF2835_MBOX_POWER_DEVID_USB_HCD);
}

/*
 * If the firmware passed a device tree use it for U-Boot.
 */
void *board_fdt_blob_setup(int *err)
{
	*err = 0;
	if (fdt_magic(fw_dtb_pointer) != FDT_MAGIC) {
		*err = -ENXIO;
		return NULL;
	}

	return (void *)fw_dtb_pointer;
}

int copy_property(void *dst, void *src, char *path, char *property)
{
	int dst_offset, src_offset;
	const fdt32_t *prop;
	int len;

	src_offset = fdt_path_offset(src, path);
	dst_offset = fdt_path_offset(dst, path);

	if (src_offset < 0 || dst_offset < 0)
		return -1;

	prop = fdt_getprop(src, src_offset, property, &len);
	if (!prop)
		return -1;

	return fdt_setprop(dst, dst_offset, property, prop, len);
}

/* Copy tweaks from the firmware dtb to the loaded dtb */
void  update_fdt_from_fw(void *fdt, void *fw_fdt)
{
	/* Using dtb from firmware directly; leave it alone */
	if (fdt == fw_fdt)
		return;

	/* The firmware provides a more precie model; so copy that */
	copy_property(fdt, fw_fdt, "/", "model");

	/* memory reserve as suggested by the firmware */
	copy_property(fdt, fw_fdt, "/", "memreserve");

	/* Adjust dma-ranges for the SD card and PCI bus as they can depend on
	 * the SoC revision
	 */
	copy_property(fdt, fw_fdt, "emmc2bus", "dma-ranges");
	copy_property(fdt, fw_fdt, "pcie0", "dma-ranges");

	/* Bootloader configuration template exposes as nvmem */
	if (copy_property(fdt, fw_fdt, "blconfig", "reg") == 0)
		copy_property(fdt, fw_fdt, "blconfig", "status");

	/* kernel address randomisation seed as provided by the firmware */
	copy_property(fdt, fw_fdt, "/chosen", "kaslr-seed");

	/* address of the PHY device as provided by the firmware  */
	copy_property(fdt, fw_fdt, "ethernet0/mdio@e14/ethernet-phy@1", "reg");
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	int node;

	update_fdt_from_fw(blob, (void *)fw_dtb_pointer);

	node = fdt_node_offset_by_compatible(blob, -1, "simple-framebuffer");
	if (node < 0)
		fdt_simplefb_add_node(blob);

#ifdef CONFIG_EFI_LOADER
	/* Reserve the spin table */
	efi_add_memory_map(0, CONFIG_NFUS_EFI_NR_SPIN_PAGES << EFI_PAGE_SHIFT,
			   EFI_RESERVED_MEMORY_TYPE);
#endif

	return 0;
}
