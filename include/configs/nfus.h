/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2012-2016 Stephen Warren
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>
#include <asm/arch/timer.h>

#ifndef __ASSEMBLY__
#include <asm/arch/base.h>
#endif

/* Architecture, CPU, etc.*/

/* Use SoC timer for AArch32, but architected timer for AArch64 */
#ifndef CONFIG_ARM64
#define CFG_SYS_TIMER_RATE		1000000
#define CFG_SYS_TIMER_COUNTER	\
	(&((struct nf2835_timer_regs *)NF2835_TIMER_PHYSADDR)->clo)
#endif

/* Memory layout */
#define CFG_SYS_SDRAM_BASE		0x00000000
#define CFG_SYS_UBOOT_BASE		CONFIG_TEXT_BASE
/*
 * The board really has 256M. However, the VC (VideoCore co-processor) shares
 * the RAM, and uses a configurable portion at the top. We tell U-Boot that a
 * smaller amount of RAM is present in order to avoid stomping on the area
 * the VC uses.
 */
#define CFG_SYS_SDRAM_SIZE		SZ_128M

/* Devices */
/* LCD */

/* DFU over USB/UDC */
#ifdef CONFIG_CMD_DFU
#ifdef CONFIG_ARM64
#define KERNEL_FILENAME		"Image"
#else
#define KERNEL_FILENAME		"zImage"
#endif

#define ENV_DFU_SETTINGS \
	"dfu_alt_info=u-boot.bin fat 0 1;uboot.env fat 0 1;" \
		      "config.txt fat 0 1;" \
		      KERNEL_FILENAME " fat 0 1\0"
#else
#define ENV_DFU_SETTINGS ""
#endif

/* Console configuration */

/* Environment */

/* Shell */

/* Environment */
#define ENV_DEVICE_SETTINGS \
	"stdin=serial,usbkbd\0" \
	"stdout=serial,vidconsole\0" \
	"stderr=serial,vidconsole\0"

#ifdef CONFIG_ARM64
#define FDT_HIGH "ffffffffffffffff"
#define INITRD_HIGH "ffffffffffffffff"
#else
#define FDT_HIGH "ffffffff"
#define INITRD_HIGH "ffffffff"
#endif

/*
 * Memory layout for where various images get loaded by boot scripts:
 *
 * I suspect address 0 is used as the SMP pen on the RPi2, so avoid this.
 *
 * Older versions of the boot firmware place the firmware-loaded DTB at 0x100,
 * newer versions place it in high memory. So prevent U-Boot from doing its own
 * DTB + initrd relocation so that we won't accidentally relocate the initrd
 * over the firmware-loaded DTB and generally try to lay out things starting
 * from the bottom of RAM.
 *
 * kernel_addr_r has different constraints on ARM and Aarch64.  For 32-bit ARM,
 * it must be within the first 128M of RAM in order for the kernel's
 * CONFIG_AUTO_ZRELADDR option to work. The kernel itself will be decompressed
 * to 0x8000 but the decompressor clobbers 0x4000-0x8000 as well. The
 * decompressor also likes to relocate itself to right past the end of the
 * decompressed kernel, so in total the sum of the compressed and and
 * decompressed kernel needs to be reserved.
 *
 *   For Aarch64, the kernel image is uncompressed and must be loaded at
 *   text_offset bytes (specified in the header of the Image) into a 2MB
 *   boundary. The 'booti' command relocates the image if necessary. Linux uses
 *   a default text_offset of 0x80000.  In summary, loading at 0x80000
 *   satisfies all these constraints and reserving memory up to 0x02400000
 *   permits fairly large (roughly 36M) kernels.
 *
 * scriptaddr and pxefile_addr_r can be pretty much anywhere that doesn't
 * conflict with something else. Reserving 1M for each of them at
 * 0x02400000-0x02500000 and 0x02500000-0x02600000 should be plenty.
 *
 * On ARM, both the DTB and any possible initrd must be loaded such that they
 * fit inside the lowmem mapping in Linux. In practice, this usually means not
 * more than ~700M away from the start of the kernel image but this number can
 * be larger OR smaller depending on e.g. the 'vmalloc=xxxM' command line
 * parameter given to the kernel. So reserving memory from low to high
 * satisfies this constraint again. Reserving 1M at 0x02600000-0x02700000 for
 * the DTB leaves rest of the free RAM to the initrd starting at 0x02700000.
 * Even with the smallest possible CPU-GPU memory split of the CPU getting
 * only 64M, the remaining 25M starting at 0x02700000 should allow quite
 * large initrds before they start colliding with U-Boot.
 */
#define ENV_MEM_LAYOUT_SETTINGS \
	"fdt_high=" FDT_HIGH "\0" \
	"initrd_high=" INITRD_HIGH "\0" \
	"kernel_addr_r=0x00080000\0" \
	"scriptaddr=0x02400000\0" \
	"pxefile_addr_r=0x02500000\0" \
	"fdt_addr_r=0x02600000\0" \
	"ramdisk_addr_r=0x02700000\0"

#if IS_ENABLED(CONFIG_CMD_MMC)
	#define BOOT_TARGET_MMC(func) \
		func(MMC, mmc, 0) \
		func(MMC, mmc, 1) \
		func(MMC, mmc, 2)
#else
	#define BOOT_TARGET_MMC(func)
#endif

#if IS_ENABLED(CONFIG_CMD_USB)
	#define BOOT_TARGET_USB(func) func(USB, usb, 0)
#else
	#define BOOT_TARGET_USB(func)
#endif

#if CONFIG_IS_ENABLED(CMD_PXE)
	#define BOOT_TARGET_PXE(func) func(PXE, pxe, na)
#else
	#define BOOT_TARGET_PXE(func)
#endif

#if CONFIG_IS_ENABLED(CMD_DHCP)
	#define BOOT_TARGET_DHCP(func) func(DHCP, dhcp, na)
#else
	#define BOOT_TARGET_DHCP(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_MMC(func) \
	BOOT_TARGET_USB(func) \
	BOOT_TARGET_PXE(func) \
	BOOT_TARGET_DHCP(func)

#include <config_distro_bootcmd.h>

#define CFG_EXTRA_ENV_SETTINGS \
	"dhcpuboot=usb start; dhcp u-boot.uimg; bootm\0" \
	ENV_DEVICE_SETTINGS \
	ENV_DFU_SETTINGS \
	ENV_MEM_LAYOUT_SETTINGS \
	BOOTENV


#endif
