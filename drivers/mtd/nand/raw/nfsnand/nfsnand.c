// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright © 2010-2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/io.h>
#include <memalign.h>
#include <nand.h>
#include <clk.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/log2.h>
#include <linux/mtd/rawnand.h>
#include <asm/processor.h>
#include <dm.h>

#include "nfsnand.h"
#include "nfsnand_compat.h"

/*
 * This flag controls if WP stays on between erase/write commands to mitigate
 * flash corruption due to power glitches. Values:
 * 0: NAND_WP is not used or not available
 * 1: NAND_WP is set by default, cleared for erase/write operations
 * 2: NAND_WP is always cleared
 */
static int wp_on = 1;
module_param(wp_on, int, 0444);

/***********************************************************************
 * Definitions
 ***********************************************************************/

#define DRV_NAME			"nfsnand"

#define CMD_NULL			0x00
#define CMD_PAGE_READ			0x01
#define CMD_SPARE_AREA_READ		0x02
#define CMD_STATUS_READ			0x03
#define CMD_PROGRAM_PAGE		0x04
#define CMD_PROGRAM_SPARE_AREA		0x05
#define CMD_COPY_BACK			0x06
#define CMD_DEVICE_ID_READ		0x07
#define CMD_BLOCK_ERASE			0x08
#define CMD_FLASH_RESET			0x09
#define CMD_BLOCKS_LOCK			0x0a
#define CMD_BLOCKS_LOCK_DOWN		0x0b
#define CMD_BLOCKS_UNLOCK		0x0c
#define CMD_READ_BLOCKS_LOCK_STATUS	0x0d
#define CMD_PARAMETER_READ		0x0e
#define CMD_PARAMETER_CHANGE_COL	0x0f
#define CMD_LOW_LEVEL_OP		0x10

struct nfs_nand_dma_desc {
	u32 next_desc;
	u32 next_desc_ext;
	u32 cmd_irq;
	u32 dram_addr;
	u32 dram_addr_ext;
	u32 tfr_len;
	u32 total_len;
	u32 flash_addr;
	u32 flash_addr_ext;
	u32 cs;
	u32 pad2[5];
	u32 status_valid;
} __packed;

/* Bitfields for nfs_nand_dma_desc::status_valid */
#define FLASH_DMA_ECC_ERROR	(1 << 8)
#define FLASH_DMA_CORR_ERROR	(1 << 9)

/* Bitfields for DMA_MODE */
#define FLASH_DMA_MODE_STOP_ON_ERROR	BIT(1) /* stop in Uncorr ECC error */
#define FLASH_DMA_MODE_MODE		BIT(0) /* link list */
#define FLASH_DMA_MODE_MASK		(FLASH_DMA_MODE_STOP_ON_ERROR |	\
						FLASH_DMA_MODE_MODE)

/* 512B flash cache in the NAND controller HW */
#define FC_SHIFT		9U
#define FC_BYTES		512U
#define FC_WORDS		(FC_BYTES >> 2)

#define NFSNAND_MIN_PAGESIZE	512
#define NFSNAND_MIN_BLOCKSIZE	(8 * 1024)
#define NFSNAND_MIN_DEVSIZE	(4ULL * 1024 * 1024)

#define NAND_CTRL_RDY			(INTFC_CTLR_READY | INTFC_FLASH_READY)
#define NAND_POLL_STATUS_TIMEOUT_MS	100

/* flash_dma registers */
enum flash_dma_reg {
	FLASH_DMA_REVISION = 0,
	FLASH_DMA_FIRST_DESC,
	FLASH_DMA_FIRST_DESC_EXT,
	FLASH_DMA_CTRL,
	FLASH_DMA_MODE,
	FLASH_DMA_STATUS,
	FLASH_DMA_INTERRUPT_DESC,
	FLASH_DMA_INTERRUPT_DESC_EXT,
	FLASH_DMA_ERROR_STATUS,
	FLASH_DMA_CURRENT_DESC,
	FLASH_DMA_CURRENT_DESC_EXT,
};

#ifndef __UBOOT__
/* flash_dma registers v0*/
static const u16 flash_dma_regs_v0[] = {
	[FLASH_DMA_REVISION]		= 0x00,
	[FLASH_DMA_FIRST_DESC]		= 0x04,
	[FLASH_DMA_CTRL]		= 0x08,
	[FLASH_DMA_MODE]		= 0x0c,
	[FLASH_DMA_STATUS]		= 0x10,
	[FLASH_DMA_INTERRUPT_DESC]	= 0x14,
	[FLASH_DMA_ERROR_STATUS]	= 0x18,
	[FLASH_DMA_CURRENT_DESC]	= 0x1c,
};

/* flash_dma registers v1*/
static const u16 flash_dma_regs_v1[] = {
	[FLASH_DMA_REVISION]		= 0x00,
	[FLASH_DMA_FIRST_DESC]		= 0x04,
	[FLASH_DMA_FIRST_DESC_EXT]	= 0x08,
	[FLASH_DMA_CTRL]		= 0x0c,
	[FLASH_DMA_MODE]		= 0x10,
	[FLASH_DMA_STATUS]		= 0x14,
	[FLASH_DMA_INTERRUPT_DESC]	= 0x18,
	[FLASH_DMA_INTERRUPT_DESC_EXT]	= 0x1c,
	[FLASH_DMA_ERROR_STATUS]	= 0x20,
	[FLASH_DMA_CURRENT_DESC]	= 0x24,
	[FLASH_DMA_CURRENT_DESC_EXT]	= 0x28,
};

/* flash_dma registers v4 */
static const u16 flash_dma_regs_v4[] = {
	[FLASH_DMA_REVISION]		= 0x00,
	[FLASH_DMA_FIRST_DESC]		= 0x08,
	[FLASH_DMA_FIRST_DESC_EXT]	= 0x0c,
	[FLASH_DMA_CTRL]		= 0x10,
	[FLASH_DMA_MODE]		= 0x14,
	[FLASH_DMA_STATUS]		= 0x18,
	[FLASH_DMA_INTERRUPT_DESC]	= 0x20,
	[FLASH_DMA_INTERRUPT_DESC_EXT]	= 0x24,
	[FLASH_DMA_ERROR_STATUS]	= 0x28,
	[FLASH_DMA_CURRENT_DESC]	= 0x30,
	[FLASH_DMA_CURRENT_DESC_EXT]	= 0x34,
};
#endif /* __UBOOT__ */

/* Controller feature flags */
enum {
	NFSNAND_HAS_1K_SECTORS			= BIT(0),
	NFSNAND_HAS_PREFETCH			= BIT(1),
	NFSNAND_HAS_CACHE_MODE			= BIT(2),
	NFSNAND_HAS_WP				= BIT(3),
};

struct nfsnand_controller {
#ifndef __UBOOT__
	struct device		*dev;
#else
	struct udevice		*dev;
#endif /* __UBOOT__ */
	struct nand_hw_control	controller;
	void __iomem		*nand_base;
	void __iomem		*nand_fc; /* flash cache */
	void __iomem		*flash_dma_base;
	unsigned int		irq;
	unsigned int		dma_irq;
	int			nand_version;
	int			parameter_page_big_endian;

	/* Some SoCs provide custom interrupt status register(s) */
	struct nfsnand_soc	*soc;

	/* Some SoCs have a gateable clock for the controller */
	struct clk		*clk;

	int			cmd_pending;
	bool			dma_pending;
	struct completion	done;
	struct completion	dma_done;

	/* List of NAND hosts (one for each chip-select) */
	struct list_head host_list;

	/* flash_dma reg */
	const u16		*flash_dma_offsets;
	struct nfs_nand_dma_desc *dma_desc;
	dma_addr_t		dma_pa;

	/* in-memory cache of the FLASH_CACHE, used only for some commands */
	u8			flash_cache[FC_BYTES];

	/* Controller revision details */
	const u16		*reg_offsets;
	unsigned int		reg_spacing; /* between CS1, CS2, ... regs */
	const u8		*cs_offsets; /* within each chip-select */
	const u8		*cs0_offsets; /* within CS0, if different */
	unsigned int		max_block_size;
	const unsigned int	*block_sizes;
	unsigned int		max_page_size;
	const unsigned int	*page_sizes;
	unsigned int		page_size_shift;
	unsigned int		max_oob;
	u32			features;

	/* for low-power standby/resume only */
	u32			nand_cs_nand_select;
	u32			nand_cs_nand_xor;
	u32			corr_stat_threshold;
	u32			flash_dma_mode;
};

struct nfsnand_cfg {
	u64			device_size;
	unsigned int		block_size;
	unsigned int		page_size;
	unsigned int		spare_area_size;
	unsigned int		device_width;
	unsigned int		col_adr_bytes;
	unsigned int		blk_adr_bytes;
	unsigned int		ful_adr_bytes;
	unsigned int		sector_size_1k;
	unsigned int		ecc_level;
	/* use for low-power standby/resume only */
	u32			acc_control;
	u32			config;
	u32			config_ext;
	u32			timing_1;
	u32			timing_2;
};

struct nfsnand_host {
	struct list_head	node;

	struct nand_chip	chip;
#ifndef __UBOOT__
	struct platform_device	*pdev;
#else
	struct udevice	*pdev;
#endif /* __UBOOT__ */
	int			cs;

	unsigned int		last_cmd;
	unsigned int		last_byte;
	u64			last_addr;
	struct nfsnand_cfg	hwcfg;
	struct nfsnand_controller *ctrl;
};

enum nfsnand_reg {
	NFSNAND_CMD_START = 0,
	NFSNAND_CMD_EXT_ADDRESS,
	NFSNAND_CMD_ADDRESS,
	NFSNAND_INTFC_STATUS,
	NFSNAND_CS_SELECT,
	NFSNAND_CS_XOR,
	NFSNAND_LL_OP,
	NFSNAND_CS0_BASE,
	NFSNAND_CS1_BASE,		/* CS1 regs, if non-contiguous */
	NFSNAND_CORR_THRESHOLD,
	NFSNAND_CORR_THRESHOLD_EXT,
	NFSNAND_UNCORR_COUNT,
	NFSNAND_CORR_COUNT,
	NFSNAND_CORR_EXT_ADDR,
	NFSNAND_CORR_ADDR,
	NFSNAND_UNCORR_EXT_ADDR,
	NFSNAND_UNCORR_ADDR,
	NFSNAND_SEMAPHORE,
	NFSNAND_ID,
	NFSNAND_ID_EXT,
	NFSNAND_LL_RDATA,
	NFSNAND_OOB_READ_BASE,
	NFSNAND_OOB_READ_10_BASE,	/* offset 0x10, if non-contiguous */
	NFSNAND_OOB_WRITE_BASE,
	NFSNAND_OOB_WRITE_10_BASE,	/* offset 0x10, if non-contiguous */
	NFSNAND_FC_BASE,
};

/* NFSNAND v2.1-v2.2 */
static const u16 nfsnand_regs_v21[] = {
	[NFSNAND_CMD_START]		=  0x04,
	[NFSNAND_CMD_EXT_ADDRESS]	=  0x08,
	[NFSNAND_CMD_ADDRESS]		=  0x0c,
	[NFSNAND_INTFC_STATUS]		=  0x5c,
	[NFSNAND_CS_SELECT]		=  0x14,
	[NFSNAND_CS_XOR]		=  0x18,
	[NFSNAND_LL_OP]		=     0,
	[NFSNAND_CS0_BASE]		=  0x40,
	[NFSNAND_CS1_BASE]		=     0,
	[NFSNAND_CORR_THRESHOLD]	=     0,
	[NFSNAND_CORR_THRESHOLD_EXT]	=     0,
	[NFSNAND_UNCORR_COUNT]		=     0,
	[NFSNAND_CORR_COUNT]		=     0,
	[NFSNAND_CORR_EXT_ADDR]	=  0x60,
	[NFSNAND_CORR_ADDR]		=  0x64,
	[NFSNAND_UNCORR_EXT_ADDR]	=  0x68,
	[NFSNAND_UNCORR_ADDR]		=  0x6c,
	[NFSNAND_SEMAPHORE]		=  0x50,
	[NFSNAND_ID]			=  0x54,
	[NFSNAND_ID_EXT]		=     0,
	[NFSNAND_LL_RDATA]		=     0,
	[NFSNAND_OOB_READ_BASE]	=  0x20,
	[NFSNAND_OOB_READ_10_BASE]	=     0,
	[NFSNAND_OOB_WRITE_BASE]	=  0x30,
	[NFSNAND_OOB_WRITE_10_BASE]	=     0,
	[NFSNAND_FC_BASE]		= 0x200,
};

/* NFSNAND v3.3-v4.0 */
static const u16 nfsnand_regs_v33[] = {
	[NFSNAND_CMD_START]		=  0x04,
	[NFSNAND_CMD_EXT_ADDRESS]	=  0x08,
	[NFSNAND_CMD_ADDRESS]		=  0x0c,
	[NFSNAND_INTFC_STATUS]		=  0x6c,
	[NFSNAND_CS_SELECT]		=  0x14,
	[NFSNAND_CS_XOR]		=  0x18,
	[NFSNAND_LL_OP]		= 0x178,
	[NFSNAND_CS0_BASE]		=  0x40,
	[NFSNAND_CS1_BASE]		=  0xd0,
	[NFSNAND_CORR_THRESHOLD]	=  0x84,
	[NFSNAND_CORR_THRESHOLD_EXT]	=     0,
	[NFSNAND_UNCORR_COUNT]		=     0,
	[NFSNAND_CORR_COUNT]		=     0,
	[NFSNAND_CORR_EXT_ADDR]	=  0x70,
	[NFSNAND_CORR_ADDR]		=  0x74,
	[NFSNAND_UNCORR_EXT_ADDR]	=  0x78,
	[NFSNAND_UNCORR_ADDR]		=  0x7c,
	[NFSNAND_SEMAPHORE]		=  0x58,
	[NFSNAND_ID]			=  0x60,
	[NFSNAND_ID_EXT]		=  0x64,
	[NFSNAND_LL_RDATA]		= 0x17c,
	[NFSNAND_OOB_READ_BASE]	=  0x20,
	[NFSNAND_OOB_READ_10_BASE]	= 0x130,
	[NFSNAND_OOB_WRITE_BASE]	=  0x30,
	[NFSNAND_OOB_WRITE_10_BASE]	=     0,
	[NFSNAND_FC_BASE]		= 0x200,
};

/* NFSNAND v5.0 */
static const u16 nfsnand_regs_v50[] = {
	[NFSNAND_CMD_START]		=  0x04,
	[NFSNAND_CMD_EXT_ADDRESS]	=  0x08,
	[NFSNAND_CMD_ADDRESS]		=  0x0c,
	[NFSNAND_INTFC_STATUS]		=  0x6c,
	[NFSNAND_CS_SELECT]		=  0x14,
	[NFSNAND_CS_XOR]		=  0x18,
	[NFSNAND_LL_OP]		= 0x178,
	[NFSNAND_CS0_BASE]		=  0x40,
	[NFSNAND_CS1_BASE]		=  0xd0,
	[NFSNAND_CORR_THRESHOLD]	=  0x84,
	[NFSNAND_CORR_THRESHOLD_EXT]	=     0,
	[NFSNAND_UNCORR_COUNT]		=     0,
	[NFSNAND_CORR_COUNT]		=     0,
	[NFSNAND_CORR_EXT_ADDR]	=  0x70,
	[NFSNAND_CORR_ADDR]		=  0x74,
	[NFSNAND_UNCORR_EXT_ADDR]	=  0x78,
	[NFSNAND_UNCORR_ADDR]		=  0x7c,
	[NFSNAND_SEMAPHORE]		=  0x58,
	[NFSNAND_ID]			=  0x60,
	[NFSNAND_ID_EXT]		=  0x64,
	[NFSNAND_LL_RDATA]		= 0x17c,
	[NFSNAND_OOB_READ_BASE]	=  0x20,
	[NFSNAND_OOB_READ_10_BASE]	= 0x130,
	[NFSNAND_OOB_WRITE_BASE]	=  0x30,
	[NFSNAND_OOB_WRITE_10_BASE]	= 0x140,
	[NFSNAND_FC_BASE]		= 0x200,
};

/* NFSNAND v6.0 - v7.1 */
static const u16 nfsnand_regs_v60[] = {
	[NFSNAND_CMD_START]		=  0x04,
	[NFSNAND_CMD_EXT_ADDRESS]	=  0x08,
	[NFSNAND_CMD_ADDRESS]		=  0x0c,
	[NFSNAND_INTFC_STATUS]		=  0x14,
	[NFSNAND_CS_SELECT]		=  0x18,
	[NFSNAND_CS_XOR]		=  0x1c,
	[NFSNAND_LL_OP]		=  0x20,
	[NFSNAND_CS0_BASE]		=  0x50,
	[NFSNAND_CS1_BASE]		=     0,
	[NFSNAND_CORR_THRESHOLD]	=  0xc0,
	[NFSNAND_CORR_THRESHOLD_EXT]	=  0xc4,
	[NFSNAND_UNCORR_COUNT]		=  0xfc,
	[NFSNAND_CORR_COUNT]		= 0x100,
	[NFSNAND_CORR_EXT_ADDR]	= 0x10c,
	[NFSNAND_CORR_ADDR]		= 0x110,
	[NFSNAND_UNCORR_EXT_ADDR]	= 0x114,
	[NFSNAND_UNCORR_ADDR]		= 0x118,
	[NFSNAND_SEMAPHORE]		= 0x150,
	[NFSNAND_ID]			= 0x194,
	[NFSNAND_ID_EXT]		= 0x198,
	[NFSNAND_LL_RDATA]		= 0x19c,
	[NFSNAND_OOB_READ_BASE]	= 0x200,
	[NFSNAND_OOB_READ_10_BASE]	=     0,
	[NFSNAND_OOB_WRITE_BASE]	= 0x280,
	[NFSNAND_OOB_WRITE_10_BASE]	=     0,
	[NFSNAND_FC_BASE]		= 0x400,
};

/* NFSNAND v7.1 */
static const u16 nfsnand_regs_v71[] = {
	[NFSNAND_CMD_START]		=  0x04,
	[NFSNAND_CMD_EXT_ADDRESS]	=  0x08,
	[NFSNAND_CMD_ADDRESS]		=  0x0c,
	[NFSNAND_INTFC_STATUS]		=  0x14,
	[NFSNAND_CS_SELECT]		=  0x18,
	[NFSNAND_CS_XOR]		=  0x1c,
	[NFSNAND_LL_OP]		=  0x20,
	[NFSNAND_CS0_BASE]		=  0x50,
	[NFSNAND_CS1_BASE]		=     0,
	[NFSNAND_CORR_THRESHOLD]	=  0xdc,
	[NFSNAND_CORR_THRESHOLD_EXT]	=  0xe0,
	[NFSNAND_UNCORR_COUNT]		=  0xfc,
	[NFSNAND_CORR_COUNT]		= 0x100,
	[NFSNAND_CORR_EXT_ADDR]	= 0x10c,
	[NFSNAND_CORR_ADDR]		= 0x110,
	[NFSNAND_UNCORR_EXT_ADDR]	= 0x114,
	[NFSNAND_UNCORR_ADDR]		= 0x118,
	[NFSNAND_SEMAPHORE]		= 0x150,
	[NFSNAND_ID]			= 0x194,
	[NFSNAND_ID_EXT]		= 0x198,
	[NFSNAND_LL_RDATA]		= 0x19c,
	[NFSNAND_OOB_READ_BASE]	= 0x200,
	[NFSNAND_OOB_READ_10_BASE]	=     0,
	[NFSNAND_OOB_WRITE_BASE]	= 0x280,
	[NFSNAND_OOB_WRITE_10_BASE]	=     0,
	[NFSNAND_FC_BASE]		= 0x400,
};

/* NFSNAND v7.2 */
static const u16 nfsnand_regs_v72[] = {
	[NFSNAND_CMD_START]		=  0x04,
	[NFSNAND_CMD_EXT_ADDRESS]	=  0x08,
	[NFSNAND_CMD_ADDRESS]		=  0x0c,
	[NFSNAND_INTFC_STATUS]		=  0x14,
	[NFSNAND_CS_SELECT]		=  0x18,
	[NFSNAND_CS_XOR]		=  0x1c,
	[NFSNAND_LL_OP]		=  0x20,
	[NFSNAND_CS0_BASE]		=  0x50,
	[NFSNAND_CS1_BASE]		=     0,
	[NFSNAND_CORR_THRESHOLD]	=  0xdc,
	[NFSNAND_CORR_THRESHOLD_EXT]	=  0xe0,
	[NFSNAND_UNCORR_COUNT]		=  0xfc,
	[NFSNAND_CORR_COUNT]		= 0x100,
	[NFSNAND_CORR_EXT_ADDR]	= 0x10c,
	[NFSNAND_CORR_ADDR]		= 0x110,
	[NFSNAND_UNCORR_EXT_ADDR]	= 0x114,
	[NFSNAND_UNCORR_ADDR]		= 0x118,
	[NFSNAND_SEMAPHORE]		= 0x150,
	[NFSNAND_ID]			= 0x194,
	[NFSNAND_ID_EXT]		= 0x198,
	[NFSNAND_LL_RDATA]		= 0x19c,
	[NFSNAND_OOB_READ_BASE]	= 0x200,
	[NFSNAND_OOB_READ_10_BASE]	=     0,
	[NFSNAND_OOB_WRITE_BASE]	= 0x400,
	[NFSNAND_OOB_WRITE_10_BASE]	=     0,
	[NFSNAND_FC_BASE]		= 0x600,
};

enum nfsnand_cs_reg {
	NFSNAND_CS_CFG_EXT = 0,
	NFSNAND_CS_CFG,
	NFSNAND_CS_ACC_CONTROL,
	NFSNAND_CS_TIMING1,
	NFSNAND_CS_TIMING2,
};

/* Per chip-select offsets for v7.1 */
static const u8 nfsnand_cs_offsets_v71[] = {
	[NFSNAND_CS_ACC_CONTROL]	= 0x00,
	[NFSNAND_CS_CFG_EXT]		= 0x04,
	[NFSNAND_CS_CFG]		= 0x08,
	[NFSNAND_CS_TIMING1]		= 0x0c,
	[NFSNAND_CS_TIMING2]		= 0x10,
};

/* Per chip-select offsets for pre v7.1, except CS0 on <= v5.0 */
static const u8 nfsnand_cs_offsets[] = {
	[NFSNAND_CS_ACC_CONTROL]	= 0x00,
	[NFSNAND_CS_CFG_EXT]		= 0x04,
	[NFSNAND_CS_CFG]		= 0x04,
	[NFSNAND_CS_TIMING1]		= 0x08,
	[NFSNAND_CS_TIMING2]		= 0x0c,
};

/* Per chip-select offset for <= v5.0 on CS0 only */
static const u8 nfsnand_cs_offsets_cs0[] = {
	[NFSNAND_CS_ACC_CONTROL]	= 0x00,
	[NFSNAND_CS_CFG_EXT]		= 0x08,
	[NFSNAND_CS_CFG]		= 0x08,
	[NFSNAND_CS_TIMING1]		= 0x10,
	[NFSNAND_CS_TIMING2]		= 0x14,
};

/*
 * Bitfields for the CFG and CFG_EXT registers. Pre-v7.1 controllers only had
 * one config register, but once the bitfields overflowed, newer controllers
 * (v7.1 and newer) added a CFG_EXT register and shuffled a few fields around.
 */
enum {
	CFG_BLK_ADR_BYTES_SHIFT		= 8,
	CFG_COL_ADR_BYTES_SHIFT		= 12,
	CFG_FUL_ADR_BYTES_SHIFT		= 16,
	CFG_BUS_WIDTH_SHIFT		= 23,
	CFG_BUS_WIDTH			= BIT(CFG_BUS_WIDTH_SHIFT),
	CFG_DEVICE_SIZE_SHIFT		= 24,

	/* Only for v2.1 */
	CFG_PAGE_SIZE_SHIFT_v2_1	= 30,

	/* Only for pre-v7.1 (with no CFG_EXT register) */
	CFG_PAGE_SIZE_SHIFT		= 20,
	CFG_BLK_SIZE_SHIFT		= 28,

	/* Only for v7.1+ (with CFG_EXT register) */
	CFG_EXT_PAGE_SIZE_SHIFT		= 0,
	CFG_EXT_BLK_SIZE_SHIFT		= 4,
};

/* NFSNAND_INTFC_STATUS */
enum {
	INTFC_FLASH_STATUS		= GENMASK(7, 0),

	INTFC_ERASED			= BIT(27),
	INTFC_OOB_VALID			= BIT(28),
	INTFC_CACHE_VALID		= BIT(29),
	INTFC_FLASH_READY		= BIT(30),
	INTFC_CTLR_READY		= BIT(31),
};

static inline u32 nand_readreg(struct nfsnand_controller *ctrl, u32 offs)
{
	return nfsnand_readl(ctrl->nand_base + offs);
}

static inline void nand_writereg(struct nfsnand_controller *ctrl, u32 offs,
				 u32 val)
{
	nfsnand_writel(val, ctrl->nand_base + offs);
}

static int nfsnand_revision_init(struct nfsnand_controller *ctrl)
{
	static const unsigned int block_sizes_v6[] = { 8, 16, 128, 256, 512, 1024, 2048, 0 };
	static const unsigned int block_sizes_v4[] = { 16, 128, 8, 512, 256, 1024, 2048, 0 };
	static const unsigned int block_sizes_v2_2[] = { 16, 128, 8, 512, 256, 0 };
	static const unsigned int block_sizes_v2_1[] = { 16, 128, 8, 512, 0 };
	static const unsigned int page_sizes_v3_4[] = { 512, 2048, 4096, 8192, 0 };
	static const unsigned int page_sizes_v2_2[] = { 512, 2048, 4096, 0 };
	static const unsigned int page_sizes_v2_1[] = { 512, 2048, 0 };

	ctrl->nand_version = nand_readreg(ctrl, 0) & 0xffff;

	/* Only support v2.1+ */
	if (ctrl->nand_version < 0x0201) {
		dev_err(ctrl->dev, "version %#x not supported\n",
			ctrl->nand_version);
		return -ENODEV;
	}

	/* Register offsets */
	if (ctrl->nand_version >= 0x0702)
		ctrl->reg_offsets = nfsnand_regs_v72;
	else if (ctrl->nand_version == 0x0701)
		ctrl->reg_offsets = nfsnand_regs_v71;
	else if (ctrl->nand_version >= 0x0600)
		ctrl->reg_offsets = nfsnand_regs_v60;
	else if (ctrl->nand_version >= 0x0500)
		ctrl->reg_offsets = nfsnand_regs_v50;
	else if (ctrl->nand_version >= 0x0303)
		ctrl->reg_offsets = nfsnand_regs_v33;
	else if (ctrl->nand_version >= 0x0201)
		ctrl->reg_offsets = nfsnand_regs_v21;

	/* Chip-select stride */
	if (ctrl->nand_version >= 0x0701)
		ctrl->reg_spacing = 0x14;
	else
		ctrl->reg_spacing = 0x10;

	/* Per chip-select registers */
	if (ctrl->nand_version >= 0x0701) {
		ctrl->cs_offsets = nfsnand_cs_offsets_v71;
	} else {
		ctrl->cs_offsets = nfsnand_cs_offsets;

		/* v3.3-5.0 have a different CS0 offset layout */
		if (ctrl->nand_version >= 0x0303 &&
		    ctrl->nand_version <= 0x0500)
			ctrl->cs0_offsets = nfsnand_cs_offsets_cs0;
	}

	/* Page / block sizes */
	if (ctrl->nand_version >= 0x0701) {
		/* >= v7.1 use nice power-of-2 values! */
		ctrl->max_page_size = 16 * 1024;
		ctrl->max_block_size = 2 * 1024 * 1024;
	} else {
		if (ctrl->nand_version >= 0x0304)
			ctrl->page_sizes = page_sizes_v3_4;
		else if (ctrl->nand_version >= 0x0202)
			ctrl->page_sizes = page_sizes_v2_2;
		else
			ctrl->page_sizes = page_sizes_v2_1;

		if (ctrl->nand_version >= 0x0202)
			ctrl->page_size_shift = CFG_PAGE_SIZE_SHIFT;
		else
			ctrl->page_size_shift = CFG_PAGE_SIZE_SHIFT_v2_1;

		if (ctrl->nand_version >= 0x0600)
			ctrl->block_sizes = block_sizes_v6;
		else if (ctrl->nand_version >= 0x0400)
			ctrl->block_sizes = block_sizes_v4;
		else if (ctrl->nand_version >= 0x0202)
			ctrl->block_sizes = block_sizes_v2_2;
		else
			ctrl->block_sizes = block_sizes_v2_1;

		if (ctrl->nand_version < 0x0400) {
			if (ctrl->nand_version < 0x0202)
				ctrl->max_page_size = 2048;
			else
				ctrl->max_page_size = 4096;
			ctrl->max_block_size = 512 * 1024;
		}
	}

	/* Maximum spare area sector size (per 512B) */
	if (ctrl->nand_version == 0x0702)
		ctrl->max_oob = 128;
	else if (ctrl->nand_version >= 0x0600)
		ctrl->max_oob = 64;
	else if (ctrl->nand_version >= 0x0500)
		ctrl->max_oob = 32;
	else
		ctrl->max_oob = 16;

	/* v6.0 and newer (except v6.1) have prefetch support */
	if (ctrl->nand_version >= 0x0600 && ctrl->nand_version != 0x0601)
		ctrl->features |= NFSNAND_HAS_PREFETCH;

	/*
	 * v6.x has cache mode, but it's implemented differently. Ignore it for
	 * now.
	 */
	if (ctrl->nand_version >= 0x0700)
		ctrl->features |= NFSNAND_HAS_CACHE_MODE;

	if (ctrl->nand_version >= 0x0500)
		ctrl->features |= NFSNAND_HAS_1K_SECTORS;

	if (ctrl->nand_version >= 0x0700)
		ctrl->features |= NFSNAND_HAS_WP;
#ifndef __UBOOT__
	else if (of_property_read_bool(ctrl->dev->of_node, "nfs,nand-has-wp"))
#else
	else if (dev_read_bool(ctrl->dev, "nfs,nand-has-wp"))
#endif /* __UBOOT__ */
		ctrl->features |= NFSNAND_HAS_WP;

	return 0;
}

#ifndef __UBOOT__
static void nfsnand_flash_dma_revision_init(struct nfsnand_controller *ctrl)
{
	/* flash_dma register offsets */
	if (ctrl->nand_version >= 0x0703)
		ctrl->flash_dma_offsets = flash_dma_regs_v4;
	else if (ctrl->nand_version == 0x0602)
		ctrl->flash_dma_offsets = flash_dma_regs_v0;
	else
		ctrl->flash_dma_offsets = flash_dma_regs_v1;
}
#endif /* __UBOOT__ */

static inline u32 nfsnand_read_reg(struct nfsnand_controller *ctrl,
		enum nfsnand_reg reg)
{
	u16 offs = ctrl->reg_offsets[reg];

	if (offs)
		return nand_readreg(ctrl, offs);
	else
		return 0;
}

static inline void nfsnand_write_reg(struct nfsnand_controller *ctrl,
				      enum nfsnand_reg reg, u32 val)
{
	u16 offs = ctrl->reg_offsets[reg];

	if (offs)
		nand_writereg(ctrl, offs, val);
}

static inline void nfsnand_rmw_reg(struct nfsnand_controller *ctrl,
				    enum nfsnand_reg reg, u32 mask, unsigned
				    int shift, u32 val)
{
	u32 tmp = nfsnand_read_reg(ctrl, reg);

	tmp &= ~mask;
	tmp |= val << shift;
	nfsnand_write_reg(ctrl, reg, tmp);
}

static inline u32 nfsnand_read_fc(struct nfsnand_controller *ctrl, int word)
{
	return __raw_readl(ctrl->nand_fc + word * 4);
}

static inline void nfsnand_write_fc(struct nfsnand_controller *ctrl,
				     int word, u32 val)
{
	__raw_writel(val, ctrl->nand_fc + word * 4);
}

static void nfsnand_clear_ecc_addr(struct nfsnand_controller *ctrl)
{

	/* Clear error addresses */
	nfsnand_write_reg(ctrl, NFSNAND_UNCORR_ADDR, 0);
	nfsnand_write_reg(ctrl, NFSNAND_CORR_ADDR, 0);
	nfsnand_write_reg(ctrl, NFSNAND_UNCORR_EXT_ADDR, 0);
	nfsnand_write_reg(ctrl, NFSNAND_CORR_EXT_ADDR, 0);
}

static u64 nfsnand_get_uncorrecc_addr(struct nfsnand_controller *ctrl)
{
	u64 err_addr;

	err_addr = nfsnand_read_reg(ctrl, NFSNAND_UNCORR_ADDR);
	err_addr |= ((u64)(nfsnand_read_reg(ctrl,
					     NFSNAND_UNCORR_EXT_ADDR)
					     & 0xffff) << 32);

	return err_addr;
}

static u64 nfsnand_get_correcc_addr(struct nfsnand_controller *ctrl)
{
	u64 err_addr;

	err_addr = nfsnand_read_reg(ctrl, NFSNAND_CORR_ADDR);
	err_addr |= ((u64)(nfsnand_read_reg(ctrl,
					     NFSNAND_CORR_EXT_ADDR)
					     & 0xffff) << 32);

	return err_addr;
}

static void nfsnand_set_cmd_addr(struct mtd_info *mtd, u64 addr)
{
	struct nand_chip *chip =  mtd_to_nand(mtd);
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;

	nfsnand_write_reg(ctrl, NFSNAND_CMD_EXT_ADDRESS,
			   (host->cs << 16) | ((addr >> 32) & 0xffff));
	(void)nfsnand_read_reg(ctrl, NFSNAND_CMD_EXT_ADDRESS);
	nfsnand_write_reg(ctrl, NFSNAND_CMD_ADDRESS,
			   lower_32_bits(addr));
	(void)nfsnand_read_reg(ctrl, NFSNAND_CMD_ADDRESS);
}

static inline u16 nfsnand_cs_offset(struct nfsnand_controller *ctrl, int cs,
				     enum nfsnand_cs_reg reg)
{
	u16 offs_cs0 = ctrl->reg_offsets[NFSNAND_CS0_BASE];
	u16 offs_cs1 = ctrl->reg_offsets[NFSNAND_CS1_BASE];
	u8 cs_offs;

	if (cs == 0 && ctrl->cs0_offsets)
		cs_offs = ctrl->cs0_offsets[reg];
	else
		cs_offs = ctrl->cs_offsets[reg];

	if (cs && offs_cs1)
		return offs_cs1 + (cs - 1) * ctrl->reg_spacing + cs_offs;

	return offs_cs0 + cs * ctrl->reg_spacing + cs_offs;
}

static inline u32 nfsnand_count_corrected(struct nfsnand_controller *ctrl)
{
	if (ctrl->nand_version < 0x0600)
		return 1;
	return nfsnand_read_reg(ctrl, NFSNAND_CORR_COUNT);
}

static void nfsnand_wr_corr_thresh(struct nfsnand_host *host, u8 val)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	unsigned int shift = 0, bits;
	enum nfsnand_reg reg = NFSNAND_CORR_THRESHOLD;
	int cs = host->cs;

	if (!ctrl->reg_offsets[reg])
		return;

	if (ctrl->nand_version == 0x0702)
		bits = 7;
	else if (ctrl->nand_version >= 0x0600)
		bits = 6;
	else if (ctrl->nand_version >= 0x0500)
		bits = 5;
	else
		bits = 4;

	if (ctrl->nand_version >= 0x0702) {
		if (cs >= 4)
			reg = NFSNAND_CORR_THRESHOLD_EXT;
		shift = (cs % 4) * bits;
	} else if (ctrl->nand_version >= 0x0600) {
		if (cs >= 5)
			reg = NFSNAND_CORR_THRESHOLD_EXT;
		shift = (cs % 5) * bits;
	}
	nfsnand_rmw_reg(ctrl, reg, (bits - 1) << shift, shift, val);
}

static inline int nfsnand_cmd_shift(struct nfsnand_controller *ctrl)
{
	if (ctrl->nand_version < 0x0602)
		return 24;
	return 0;
}

/***********************************************************************
 * NAND ACC CONTROL bitfield
 *
 * Some bits have remained constant throughout hardware revision, while
 * others have shifted around.
 ***********************************************************************/

/* Constant for all versions (where supported) */
enum {
	/* See NFSNAND_HAS_CACHE_MODE */
	ACC_CONTROL_CACHE_MODE				= BIT(22),

	/* See NFSNAND_HAS_PREFETCH */
	ACC_CONTROL_PREFETCH				= BIT(23),

	ACC_CONTROL_PAGE_HIT				= BIT(24),
	ACC_CONTROL_WR_PREEMPT				= BIT(25),
	ACC_CONTROL_PARTIAL_PAGE			= BIT(26),
	ACC_CONTROL_RD_ERASED				= BIT(27),
	ACC_CONTROL_FAST_PGM_RDIN			= BIT(28),
	ACC_CONTROL_WR_ECC				= BIT(30),
	ACC_CONTROL_RD_ECC				= BIT(31),
};

static inline u32 nfsnand_spare_area_mask(struct nfsnand_controller *ctrl)
{
	if (ctrl->nand_version == 0x0702)
		return GENMASK(7, 0);
	else if (ctrl->nand_version >= 0x0600)
		return GENMASK(6, 0);
	else if (ctrl->nand_version >= 0x0303)
		return GENMASK(5, 0);
	else
		return GENMASK(4, 0);
}

#define NAND_ACC_CONTROL_ECC_SHIFT	16
#define NAND_ACC_CONTROL_ECC_EXT_SHIFT	13

static inline u32 nfsnand_ecc_level_mask(struct nfsnand_controller *ctrl)
{
	u32 mask = (ctrl->nand_version >= 0x0600) ? 0x1f : 0x0f;

	mask <<= NAND_ACC_CONTROL_ECC_SHIFT;

	/* v7.2 includes additional ECC levels */
	if (ctrl->nand_version >= 0x0702)
		mask |= 0x7 << NAND_ACC_CONTROL_ECC_EXT_SHIFT;

	return mask;
}

static void nfsnand_set_ecc_enabled(struct nfsnand_host *host, int en)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	u16 offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_ACC_CONTROL);
	u32 acc_control = nand_readreg(ctrl, offs);
	u32 ecc_flags = ACC_CONTROL_WR_ECC | ACC_CONTROL_RD_ECC;

	if (en) {
		acc_control |= ecc_flags; /* enable RD/WR ECC */
		acc_control |= host->hwcfg.ecc_level
			       << NAND_ACC_CONTROL_ECC_SHIFT;
	} else {
		acc_control &= ~ecc_flags; /* disable RD/WR ECC */
		acc_control &= ~nfsnand_ecc_level_mask(ctrl);
	}

	nand_writereg(ctrl, offs, acc_control);
}

static inline int nfsnand_sector_1k_shift(struct nfsnand_controller *ctrl)
{
	if (ctrl->nand_version >= 0x0702)
		return 9;
	else if (ctrl->nand_version >= 0x0600)
		return 7;
	else if (ctrl->nand_version >= 0x0500)
		return 6;
	else
		return -1;
}

static int nfsnand_get_sector_size_1k(struct nfsnand_host *host)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	int shift = nfsnand_sector_1k_shift(ctrl);
	u16 acc_control_offs = nfsnand_cs_offset(ctrl, host->cs,
						  NFSNAND_CS_ACC_CONTROL);

	if (shift < 0)
		return 0;

	return (nand_readreg(ctrl, acc_control_offs) >> shift) & 0x1;
}

static void nfsnand_set_sector_size_1k(struct nfsnand_host *host, int val)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	int shift = nfsnand_sector_1k_shift(ctrl);
	u16 acc_control_offs = nfsnand_cs_offset(ctrl, host->cs,
						  NFSNAND_CS_ACC_CONTROL);
	u32 tmp;

	if (shift < 0)
		return;

	tmp = nand_readreg(ctrl, acc_control_offs);
	tmp &= ~(1 << shift);
	tmp |= (!!val) << shift;
	nand_writereg(ctrl, acc_control_offs, tmp);
}

/***********************************************************************
 * CS_NAND_SELECT
 ***********************************************************************/

enum {
	CS_SELECT_NAND_WP			= BIT(29),
	CS_SELECT_AUTO_DEVICE_ID_CFG		= BIT(30),
};

static int bcmnand_ctrl_poll_status(struct nfsnand_controller *ctrl,
				    u32 mask, u32 expected_val,
				    unsigned long timeout_ms)
{
#ifndef __UBOOT__
	unsigned long limit;
	u32 val;

	if (!timeout_ms)
		timeout_ms = NAND_POLL_STATUS_TIMEOUT_MS;

	limit = jiffies + msecs_to_jiffies(timeout_ms);
	do {
		val = nfsnand_read_reg(ctrl, NFSNAND_INTFC_STATUS);
		if ((val & mask) == expected_val)
			return 0;

		cpu_relax();
	} while (time_after(limit, jiffies));
#else
	unsigned long base, limit;
	u32 val;

	if (!timeout_ms)
		timeout_ms = NAND_POLL_STATUS_TIMEOUT_MS;

	base = get_timer(0);
	limit = CONFIG_SYS_HZ * timeout_ms / 1000;
	do {
		val = nfsnand_read_reg(ctrl, NFSNAND_INTFC_STATUS);
		if ((val & mask) == expected_val)
			return 0;

		cpu_relax();
	} while (get_timer(base) < limit);
#endif /* __UBOOT__ */

	dev_warn(ctrl->dev, "timeout on status poll (expected %x got %x)\n",
		 expected_val, val & mask);

	return -ETIMEDOUT;
}

static inline void nfsnand_set_wp(struct nfsnand_controller *ctrl, bool en)
{
	u32 val = en ? CS_SELECT_NAND_WP : 0;

	nfsnand_rmw_reg(ctrl, NFSNAND_CS_SELECT, CS_SELECT_NAND_WP, 0, val);
}

/***********************************************************************
 * Flash DMA
 ***********************************************************************/

static inline bool has_flash_dma(struct nfsnand_controller *ctrl)
{
	return ctrl->flash_dma_base;
}

static inline bool flash_dma_buf_ok(const void *buf)
{
#ifndef __UBOOT__
	return buf && !is_vmalloc_addr(buf) &&
		likely(IS_ALIGNED((uintptr_t)buf, 4));
#else
	return buf && likely(IS_ALIGNED((uintptr_t)buf, 4));
#endif /* __UBOOT__ */
}

static inline void flash_dma_writel(struct nfsnand_controller *ctrl,
				    enum flash_dma_reg dma_reg, u32 val)
{
	u16 offs = ctrl->flash_dma_offsets[dma_reg];

	nfsnand_writel(val, ctrl->flash_dma_base + offs);
}

static inline u32 flash_dma_readl(struct nfsnand_controller *ctrl,
				  enum flash_dma_reg dma_reg)
{
	u16 offs = ctrl->flash_dma_offsets[dma_reg];

	return nfsnand_readl(ctrl->flash_dma_base + offs);
}

/* Low-level operation types: command, address, write, or read */
enum nfsnand_llop_type {
	LL_OP_CMD,
	LL_OP_ADDR,
	LL_OP_WR,
	LL_OP_RD,
};

/***********************************************************************
 * Internal support functions
 ***********************************************************************/

static inline bool is_hamming_ecc(struct nfsnand_controller *ctrl,
				  struct nfsnand_cfg *cfg)
{
	if (ctrl->nand_version <= 0x0701)
		return cfg->sector_size_1k == 0 && cfg->spare_area_size == 16 &&
			cfg->ecc_level == 15;
	else
		return cfg->sector_size_1k == 0 && ((cfg->spare_area_size == 16 &&
			cfg->ecc_level == 15) ||
			(cfg->spare_area_size == 28 && cfg->ecc_level == 16));
}

/*
 * Returns a nand_ecclayout strucutre for the given layout/configuration.
 * Returns NULL on failure.
 */
static struct nand_ecclayout *nfsnand_create_layout(int ecc_level,
						     struct nfsnand_host *host)
{
	struct nfsnand_cfg *cfg = &host->hwcfg;
	int i, j;
	struct nand_ecclayout *layout;
	int req;
	int sectors;
	int sas;
	int idx1, idx2;

#ifndef __UBOOT__
	layout = devm_kzalloc(&host->pdev->dev, sizeof(*layout), GFP_KERNEL);
#else
	layout = devm_kzalloc(host->pdev, sizeof(*layout), GFP_KERNEL);
#endif
	if (!layout)
		return NULL;

	sectors = cfg->page_size / (512 << cfg->sector_size_1k);
	sas = cfg->spare_area_size << cfg->sector_size_1k;

	/* Hamming */
	if (is_hamming_ecc(host->ctrl, cfg)) {
		for (i = 0, idx1 = 0, idx2 = 0; i < sectors; i++) {
			/* First sector of each page may have BBI */
			if (i == 0) {
				layout->oobfree[idx2].offset = i * sas + 1;
				/* Small-page NAND use byte 6 for BBI */
				if (cfg->page_size == 512)
					layout->oobfree[idx2].offset--;
				layout->oobfree[idx2].length = 5;
			} else {
				layout->oobfree[idx2].offset = i * sas;
				layout->oobfree[idx2].length = 6;
			}
			idx2++;
			layout->eccpos[idx1++] = i * sas + 6;
			layout->eccpos[idx1++] = i * sas + 7;
			layout->eccpos[idx1++] = i * sas + 8;
			layout->oobfree[idx2].offset = i * sas + 9;
			layout->oobfree[idx2].length = 7;
			idx2++;
			/* Leave zero-terminated entry for OOBFREE */
			if (idx1 >= MTD_MAX_ECCPOS_ENTRIES_LARGE ||
			    idx2 >= MTD_MAX_OOBFREE_ENTRIES_LARGE - 1)
				break;
		}

		return layout;
	}

	/*
	 * CONTROLLER_VERSION:
	 *   < v5.0: ECC_REQ = ceil(BCH_T * 13/8)
	 *  >= v5.0: ECC_REQ = ceil(BCH_T * 14/8)
	 * But we will just be conservative.
	 */
	req = DIV_ROUND_UP(ecc_level * 14, 8);
	if (req >= sas) {
		dev_err(host->pdev,
			"error: ECC too large for OOB (ECC bytes %d, spare sector %d)\n",
			req, sas);
		return NULL;
	}

	layout->eccbytes = req * sectors;
	for (i = 0, idx1 = 0, idx2 = 0; i < sectors; i++) {
		for (j = sas - req; j < sas && idx1 <
				MTD_MAX_ECCPOS_ENTRIES_LARGE; j++, idx1++)
			layout->eccpos[idx1] = i * sas + j;

		/* First sector of each page may have BBI */
		if (i == 0) {
			if (cfg->page_size == 512 && (sas - req >= 6)) {
				/* Small-page NAND use byte 6 for BBI */
				layout->oobfree[idx2].offset = 0;
				layout->oobfree[idx2].length = 5;
				idx2++;
				if (sas - req > 6) {
					layout->oobfree[idx2].offset = 6;
					layout->oobfree[idx2].length =
						sas - req - 6;
					idx2++;
				}
			} else if (sas > req + 1) {
				layout->oobfree[idx2].offset = i * sas + 1;
				layout->oobfree[idx2].length = sas - req - 1;
				idx2++;
			}
		} else if (sas > req) {
			layout->oobfree[idx2].offset = i * sas;
			layout->oobfree[idx2].length = sas - req;
			idx2++;
		}
		/* Leave zero-terminated entry for OOBFREE */
		if (idx1 >= MTD_MAX_ECCPOS_ENTRIES_LARGE ||
		    idx2 >= MTD_MAX_OOBFREE_ENTRIES_LARGE - 1)
			break;
	}

	return layout;
}

static struct nand_ecclayout *nfsstb_choose_ecc_layout(
		struct nfsnand_host *host)
{
	struct nand_ecclayout *layout;
	struct nfsnand_cfg *p = &host->hwcfg;
	unsigned int ecc_level = p->ecc_level;

	if (p->sector_size_1k)
		ecc_level <<= 1;

	layout = nfsnand_create_layout(ecc_level, host);
	if (!layout) {
		dev_err(host->pdev,
			"no proper ecc_layout for this NAND cfg\n");
		return NULL;
	}

	return layout;
}

static void nfsnand_wp(struct mtd_info *mtd, int wp)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;

	if ((ctrl->features & NFSNAND_HAS_WP) && wp_on == 1) {
		static int old_wp = -1;
		int ret;

		if (old_wp != wp) {
			dev_dbg(ctrl->dev, "WP %s\n", wp ? "on" : "off");
			old_wp = wp;
		}

		/*
		 * make sure ctrl/flash ready before and after
		 * changing state of #WP pin
		 */
		ret = bcmnand_ctrl_poll_status(ctrl, NAND_CTRL_RDY |
					       NAND_STATUS_READY,
					       NAND_CTRL_RDY |
					       NAND_STATUS_READY, 0);
		if (ret)
			return;

		nfsnand_set_wp(ctrl, wp);
		nand_status_op(chip, NULL);
		/* NAND_STATUS_WP 0x00 = protected, 0x80 = not protected */
		ret = bcmnand_ctrl_poll_status(ctrl,
					       NAND_CTRL_RDY |
					       NAND_STATUS_READY |
					       NAND_STATUS_WP,
					       NAND_CTRL_RDY |
					       NAND_STATUS_READY |
					       (wp ? 0 : NAND_STATUS_WP), 0);
		if (ret)
			dev_err(host->pdev, "nand #WP expected %s\n",
				wp ? "on" : "off");
	}
}

/* Helper functions for reading and writing OOB registers */
static inline u8 oob_reg_read(struct nfsnand_controller *ctrl, u32 offs)
{
	u16 offset0, offset10, reg_offs;

	offset0 = ctrl->reg_offsets[NFSNAND_OOB_READ_BASE];
	offset10 = ctrl->reg_offsets[NFSNAND_OOB_READ_10_BASE];

	if (offs >= ctrl->max_oob)
		return 0x77;

	if (offs >= 16 && offset10)
		reg_offs = offset10 + ((offs - 0x10) & ~0x03);
	else
		reg_offs = offset0 + (offs & ~0x03);

	return nand_readreg(ctrl, reg_offs) >> (24 - ((offs & 0x03) << 3));
}

static inline void oob_reg_write(struct nfsnand_controller *ctrl, u32 offs,
				 u32 data)
{
	u16 offset0, offset10, reg_offs;

	offset0 = ctrl->reg_offsets[NFSNAND_OOB_WRITE_BASE];
	offset10 = ctrl->reg_offsets[NFSNAND_OOB_WRITE_10_BASE];

	if (offs >= ctrl->max_oob)
		return;

	if (offs >= 16 && offset10)
		reg_offs = offset10 + ((offs - 0x10) & ~0x03);
	else
		reg_offs = offset0 + (offs & ~0x03);

	nand_writereg(ctrl, reg_offs, data);
}

/*
 * read_oob_from_regs - read data from OOB registers
 * @ctrl: NAND controller
 * @i: sub-page sector index
 * @oob: buffer to read to
 * @sas: spare area sector size (i.e., OOB size per FLASH_CACHE)
 * @sector_1k: 1 for 1KiB sectors, 0 for 512B, other values are illegal
 */
static int read_oob_from_regs(struct nfsnand_controller *ctrl, int i, u8 *oob,
			      int sas, int sector_1k)
{
	int tbytes = sas << sector_1k;
	int j;

	/* Adjust OOB values for 1K sector size */
	if (sector_1k && (i & 0x01))
		tbytes = max(0, tbytes - (int)ctrl->max_oob);
	tbytes = min_t(int, tbytes, ctrl->max_oob);

	for (j = 0; j < tbytes; j++)
		oob[j] = oob_reg_read(ctrl, j);
	return tbytes;
}

/*
 * write_oob_to_regs - write data to OOB registers
 * @i: sub-page sector index
 * @oob: buffer to write from
 * @sas: spare area sector size (i.e., OOB size per FLASH_CACHE)
 * @sector_1k: 1 for 1KiB sectors, 0 for 512B, other values are illegal
 */
static int write_oob_to_regs(struct nfsnand_controller *ctrl, int i,
			     const u8 *oob, int sas, int sector_1k)
{
	int tbytes = sas << sector_1k;
	int j;

	/* Adjust OOB values for 1K sector size */
	if (sector_1k && (i & 0x01))
		tbytes = max(0, tbytes - (int)ctrl->max_oob);
	tbytes = min_t(int, tbytes, ctrl->max_oob);

	for (j = 0; j < tbytes; j += 4)
		oob_reg_write(ctrl, j,
				(oob[j + 0] << 24) |
				(oob[j + 1] << 16) |
				(oob[j + 2] <<  8) |
				(oob[j + 3] <<  0));
	return tbytes;
}

#ifndef __UBOOT__
static irqreturn_t nfsnand_ctlrdy_irq(int irq, void *data)
{
	struct nfsnand_controller *ctrl = data;

	/* Discard all NAND_CTLRDY interrupts during DMA */
	if (ctrl->dma_pending)
		return IRQ_HANDLED;

	complete(&ctrl->done);
	return IRQ_HANDLED;
}

/* Handle SoC-specific interrupt hardware */
static irqreturn_t nfsnand_irq(int irq, void *data)
{
	struct nfsnand_controller *ctrl = data;

	if (ctrl->soc->ctlrdy_ack(ctrl->soc))
		return nfsnand_ctlrdy_irq(irq, data);

	return IRQ_NONE;
}

static irqreturn_t nfsnand_dma_irq(int irq, void *data)
{
	struct nfsnand_controller *ctrl = data;

	complete(&ctrl->dma_done);

	return IRQ_HANDLED;
}
#endif /* __UBOOT__ */

static void nfsnand_send_cmd(struct nfsnand_host *host, int cmd)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	int ret;
	u64 cmd_addr;

	cmd_addr = nfsnand_read_reg(ctrl, NFSNAND_CMD_ADDRESS);

	dev_dbg(ctrl->dev, "send native cmd %d addr 0x%llx\n", cmd, cmd_addr);

	BUG_ON(ctrl->cmd_pending != 0);
	ctrl->cmd_pending = cmd;

	ret = bcmnand_ctrl_poll_status(ctrl, NAND_CTRL_RDY, NAND_CTRL_RDY, 0);
	WARN_ON(ret);

	mb(); /* flush previous writes */
	nfsnand_write_reg(ctrl, NFSNAND_CMD_START,
			   cmd << nfsnand_cmd_shift(ctrl));
}

/***********************************************************************
 * NAND MTD API: read/program/erase
 ***********************************************************************/

static void nfsnand_cmd_ctrl(struct mtd_info *mtd, int dat,
	unsigned int ctrl)
{
	/* intentionally left blank */
}

static int nfsnand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;

#ifndef __UBOOT__
	unsigned long timeo = msecs_to_jiffies(100);

	dev_dbg(ctrl->dev, "wait on native cmd %d\n", ctrl->cmd_pending);
	if (ctrl->cmd_pending &&
			wait_for_completion_timeout(&ctrl->done, timeo) <= 0) {
		u32 cmd = nfsnand_read_reg(ctrl, NFSNAND_CMD_START)
					>> nfsnand_cmd_shift(ctrl);

		dev_err_ratelimited(ctrl->dev,
			"timeout waiting for command %#02x\n", cmd);
		dev_err_ratelimited(ctrl->dev, "intfc status %08x\n",
			nfsnand_read_reg(ctrl, NFSNAND_INTFC_STATUS));
	}
#else
	unsigned long timeo = 100; /* 100 msec */
	int ret;

	dev_dbg(ctrl->dev, "wait on native cmd %d\n", ctrl->cmd_pending);

	ret = bcmnand_ctrl_poll_status(ctrl, NAND_CTRL_RDY, NAND_CTRL_RDY, timeo);
	WARN_ON(ret);
#endif /* __UBOOT__ */

	ctrl->cmd_pending = 0;
	return nfsnand_read_reg(ctrl, NFSNAND_INTFC_STATUS) &
				 INTFC_FLASH_STATUS;
}

enum {
	LLOP_RE				= BIT(16),
	LLOP_WE				= BIT(17),
	LLOP_ALE			= BIT(18),
	LLOP_CLE			= BIT(19),
	LLOP_RETURN_IDLE		= BIT(31),

	LLOP_DATA_MASK			= GENMASK(15, 0),
};

static int nfsnand_low_level_op(struct nfsnand_host *host,
				 enum nfsnand_llop_type type, u32 data,
				 bool last_op)
{
	struct mtd_info *mtd = nand_to_mtd(&host->chip);
	struct nand_chip *chip = &host->chip;
	struct nfsnand_controller *ctrl = host->ctrl;
	u32 tmp;

	tmp = data & LLOP_DATA_MASK;
	switch (type) {
	case LL_OP_CMD:
		tmp |= LLOP_WE | LLOP_CLE;
		break;
	case LL_OP_ADDR:
		/* WE | ALE */
		tmp |= LLOP_WE | LLOP_ALE;
		break;
	case LL_OP_WR:
		/* WE */
		tmp |= LLOP_WE;
		break;
	case LL_OP_RD:
		/* RE */
		tmp |= LLOP_RE;
		break;
	}
	if (last_op)
		/* RETURN_IDLE */
		tmp |= LLOP_RETURN_IDLE;

	dev_dbg(ctrl->dev, "ll_op cmd %#x\n", tmp);

	nfsnand_write_reg(ctrl, NFSNAND_LL_OP, tmp);
	(void)nfsnand_read_reg(ctrl, NFSNAND_LL_OP);

	nfsnand_send_cmd(host, CMD_LOW_LEVEL_OP);
	return nfsnand_waitfunc(mtd, chip);
}

static void nfsnand_cmdfunc(struct mtd_info *mtd, unsigned command,
			     int column, int page_addr)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;
	u64 addr = (u64)page_addr << chip->page_shift;
	int native_cmd = 0;

	if (command == NAND_CMD_READID || command == NAND_CMD_PARAM ||
			command == NAND_CMD_RNDOUT)
		addr = (u64)column;
	/* Avoid propagating a negative, don't-care address */
	else if (page_addr < 0)
		addr = 0;

	dev_dbg(ctrl->dev, "cmd 0x%x addr 0x%llx\n", command,
		(unsigned long long)addr);

	host->last_cmd = command;
	host->last_byte = 0;
	host->last_addr = addr;

	switch (command) {
	case NAND_CMD_RESET:
		native_cmd = CMD_FLASH_RESET;
		break;
	case NAND_CMD_STATUS:
		native_cmd = CMD_STATUS_READ;
		break;
	case NAND_CMD_READID:
		native_cmd = CMD_DEVICE_ID_READ;
		break;
	case NAND_CMD_READOOB:
		native_cmd = CMD_SPARE_AREA_READ;
		break;
	case NAND_CMD_ERASE1:
		native_cmd = CMD_BLOCK_ERASE;
		nfsnand_wp(mtd, 0);
		break;
	case NAND_CMD_PARAM:
		native_cmd = CMD_PARAMETER_READ;
		break;
	case NAND_CMD_SET_FEATURES:
	case NAND_CMD_GET_FEATURES:
		nfsnand_low_level_op(host, LL_OP_CMD, command, false);
		nfsnand_low_level_op(host, LL_OP_ADDR, column, false);
		break;
	case NAND_CMD_RNDOUT:
		native_cmd = CMD_PARAMETER_CHANGE_COL;
		addr &= ~((u64)(FC_BYTES - 1));
		/*
		 * HW quirk: PARAMETER_CHANGE_COL requires SECTOR_SIZE_1K=0
		 * NB: hwcfg.sector_size_1k may not be initialized yet
		 */
		if (nfsnand_get_sector_size_1k(host)) {
			host->hwcfg.sector_size_1k =
				nfsnand_get_sector_size_1k(host);
			nfsnand_set_sector_size_1k(host, 0);
		}
		break;
	}

	if (!native_cmd)
		return;

	nfsnand_set_cmd_addr(mtd, addr);
	nfsnand_send_cmd(host, native_cmd);
	nfsnand_waitfunc(mtd, chip);

	if (native_cmd == CMD_PARAMETER_READ ||
			native_cmd == CMD_PARAMETER_CHANGE_COL) {
		/* Copy flash cache word-wise */
		u32 *flash_cache = (u32 *)ctrl->flash_cache;
		int i;

		nfsnand_soc_data_bus_prepare(ctrl->soc, true);

		/*
		 * Must cache the FLASH_CACHE now, since changes in
		 * SECTOR_SIZE_1K may invalidate it
		 */
		for (i = 0; i < FC_WORDS; i++) {
			u32 fc;

			fc = nfsnand_read_fc(ctrl, i);

			/*
			 * Flash cache is big endian for parameter pages, at
			 * least on STB SoCs
			 */
			if (ctrl->parameter_page_big_endian)
				flash_cache[i] = be32_to_cpu(fc);
			else
				flash_cache[i] = le32_to_cpu(fc);
		}

		nfsnand_soc_data_bus_unprepare(ctrl->soc, true);

		/* Cleanup from HW quirk: restore SECTOR_SIZE_1K */
		if (host->hwcfg.sector_size_1k)
			nfsnand_set_sector_size_1k(host,
						    host->hwcfg.sector_size_1k);
	}

	/* Re-enable protection is necessary only after erase */
	if (command == NAND_CMD_ERASE1)
		nfsnand_wp(mtd, 1);
}

static uint8_t nfsnand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;
	uint8_t ret = 0;
	int addr, offs;

	switch (host->last_cmd) {
	case NAND_CMD_READID:
		if (host->last_byte < 4)
			ret = nfsnand_read_reg(ctrl, NFSNAND_ID) >>
				(24 - (host->last_byte << 3));
		else if (host->last_byte < 8)
			ret = nfsnand_read_reg(ctrl, NFSNAND_ID_EXT) >>
				(56 - (host->last_byte << 3));
		break;

	case NAND_CMD_READOOB:
		ret = oob_reg_read(ctrl, host->last_byte);
		break;

	case NAND_CMD_STATUS:
		ret = nfsnand_read_reg(ctrl, NFSNAND_INTFC_STATUS) &
					INTFC_FLASH_STATUS;
		if (wp_on) /* hide WP status */
			ret |= NAND_STATUS_WP;
		break;

	case NAND_CMD_PARAM:
	case NAND_CMD_RNDOUT:
		addr = host->last_addr + host->last_byte;
		offs = addr & (FC_BYTES - 1);

		/* At FC_BYTES boundary, switch to next column */
		if (host->last_byte > 0 && offs == 0)
			nand_change_read_column_op(chip, addr, NULL, 0, false);

		ret = ctrl->flash_cache[offs];
		break;
	case NAND_CMD_GET_FEATURES:
		if (host->last_byte >= ONFI_SUBFEATURE_PARAM_LEN) {
			ret = 0;
		} else {
			bool last = host->last_byte ==
				ONFI_SUBFEATURE_PARAM_LEN - 1;
			nfsnand_low_level_op(host, LL_OP_RD, 0, last);
			ret = nfsnand_read_reg(ctrl, NFSNAND_LL_RDATA) & 0xff;
		}
	}

	dev_dbg(ctrl->dev, "read byte = 0x%02x\n", ret);
	host->last_byte++;

	return ret;
}

static void nfsnand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < len; i++, buf++)
		*buf = nfsnand_read_byte(mtd);
}

static void nfsnand_write_buf(struct mtd_info *mtd, const uint8_t *buf,
				   int len)
{
	int i;
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nfsnand_host *host = nand_get_controller_data(chip);

	switch (host->last_cmd) {
	case NAND_CMD_SET_FEATURES:
		for (i = 0; i < len; i++)
			nfsnand_low_level_op(host, LL_OP_WR, buf[i],
						  (i + 1) == len);
		break;
	default:
		BUG();
		break;
	}
}

/**
 * Construct a FLASH_DMA descriptor as part of a linked list. You must know the
 * following ahead of time:
 *  - Is this descriptor the beginning or end of a linked list?
 *  - What is the (DMA) address of the next descriptor in the linked list?
 */
#ifndef __UBOOT__
static int nfsnand_fill_dma_desc(struct nfsnand_host *host,
				  struct nfs_nand_dma_desc *desc, u64 addr,
				  dma_addr_t buf, u32 len, u8 dma_cmd,
				  bool begin, bool end,
				  dma_addr_t next_desc)
{
	memset(desc, 0, sizeof(*desc));
	/* Descriptors are written in native byte order (wordwise) */
	desc->next_desc = lower_32_bits(next_desc);
	desc->next_desc_ext = upper_32_bits(next_desc);
	desc->cmd_irq = (dma_cmd << 24) |
		(end ? (0x03 << 8) : 0) | /* IRQ | STOP */
		(!!begin) | ((!!end) << 1); /* head, tail */
#ifdef CONFIG_CPU_BIG_ENDIAN
	desc->cmd_irq |= 0x01 << 12;
#endif
	desc->dram_addr = lower_32_bits(buf);
	desc->dram_addr_ext = upper_32_bits(buf);
	desc->tfr_len = len;
	desc->total_len = len;
	desc->flash_addr = lower_32_bits(addr);
	desc->flash_addr_ext = upper_32_bits(addr);
	desc->cs = host->cs;
	desc->status_valid = 0x01;
	return 0;
}

/**
 * Kick the FLASH_DMA engine, with a given DMA descriptor
 */
static void nfsnand_dma_run(struct nfsnand_host *host, dma_addr_t desc)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	unsigned long timeo = msecs_to_jiffies(100);

	flash_dma_writel(ctrl, FLASH_DMA_FIRST_DESC, lower_32_bits(desc));
	(void)flash_dma_readl(ctrl, FLASH_DMA_FIRST_DESC);
	if (ctrl->nand_version > 0x0602) {
		flash_dma_writel(ctrl, FLASH_DMA_FIRST_DESC_EXT,
				 upper_32_bits(desc));
		(void)flash_dma_readl(ctrl, FLASH_DMA_FIRST_DESC_EXT);
	}

	/* Start FLASH_DMA engine */
	ctrl->dma_pending = true;
	mb(); /* flush previous writes */
	flash_dma_writel(ctrl, FLASH_DMA_CTRL, 0x03); /* wake | run */

	if (wait_for_completion_timeout(&ctrl->dma_done, timeo) <= 0) {
		dev_err(ctrl->dev,
				"timeout waiting for DMA; status %#x, error status %#x\n",
				flash_dma_readl(ctrl, FLASH_DMA_STATUS),
				flash_dma_readl(ctrl, FLASH_DMA_ERROR_STATUS));
	}
	ctrl->dma_pending = false;
	flash_dma_writel(ctrl, FLASH_DMA_CTRL, 0); /* force stop */
}

static int nfsnand_dma_trans(struct nfsnand_host *host, u64 addr, u32 *buf,
			      u32 len, u8 dma_cmd)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	dma_addr_t buf_pa;
	int dir = dma_cmd == CMD_PAGE_READ ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	buf_pa = dma_map_single(ctrl->dev, buf, len, dir);
	if (dma_mapping_error(ctrl->dev, buf_pa)) {
		dev_err(ctrl->dev, "unable to map buffer for DMA\n");
		return -ENOMEM;
	}

	nfsnand_fill_dma_desc(host, ctrl->dma_desc, addr, buf_pa, len,
				   dma_cmd, true, true, 0);

	nfsnand_dma_run(host, ctrl->dma_pa);

	dma_unmap_single(ctrl->dev, buf_pa, len, dir);

	if (ctrl->dma_desc->status_valid & FLASH_DMA_ECC_ERROR)
		return -EBADMSG;
	else if (ctrl->dma_desc->status_valid & FLASH_DMA_CORR_ERROR)
		return -EUCLEAN;

	return 0;
}
#endif /* __UBOOT__ */

/*
 * Assumes proper CS is already set
 */
static int nfsnand_read_by_pio(struct mtd_info *mtd, struct nand_chip *chip,
				u64 addr, unsigned int trans, u32 *buf,
				u8 *oob, u64 *err_addr)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;
	int i, j, ret = 0;

	nfsnand_clear_ecc_addr(ctrl);

	for (i = 0; i < trans; i++, addr += FC_BYTES) {
		nfsnand_set_cmd_addr(mtd, addr);
		/* SPARE_AREA_READ does not use ECC, so just use PAGE_READ */
		nfsnand_send_cmd(host, CMD_PAGE_READ);
		nfsnand_waitfunc(mtd, chip);

		if (likely(buf)) {
			nfsnand_soc_data_bus_prepare(ctrl->soc, false);

			for (j = 0; j < FC_WORDS; j++, buf++)
				*buf = nfsnand_read_fc(ctrl, j);

			nfsnand_soc_data_bus_unprepare(ctrl->soc, false);
		}

		if (oob)
			oob += read_oob_from_regs(ctrl, i, oob,
					mtd->oobsize / trans,
					host->hwcfg.sector_size_1k);

		if (ret != -EBADMSG) {
			*err_addr = nfsnand_get_uncorrecc_addr(ctrl);

			if (*err_addr)
				ret = -EBADMSG;
		}

		if (!ret) {
			*err_addr = nfsnand_get_correcc_addr(ctrl);

			if (*err_addr)
				ret = -EUCLEAN;
		}
	}

	return ret;
}

/*
 * Check a page to see if it is erased (w/ bitflips) after an uncorrectable ECC
 * error
 *
 * Because the HW ECC signals an ECC error if an erase paged has even a single
 * bitflip, we must check each ECC error to see if it is actually an erased
 * page with bitflips, not a truly corrupted page.
 *
 * On a real error, return a negative error code (-EBADMSG for ECC error), and
 * buf will contain raw data.
 * Otherwise, buf gets filled with 0xffs and return the maximum number of
 * bitflips-per-ECC-sector to the caller.
 *
 */
static int nfsstb_nand_verify_erased_page(struct mtd_info *mtd,
		  struct nand_chip *chip, void *buf, u64 addr)
{
	struct mtd_oob_region ecc;
	int i;
	int bitflips = 0;
	int page = addr >> chip->page_shift;
	int ret;
	void *ecc_bytes;
	void *ecc_chunk;

	if (!buf) {
#ifndef __UBOOT__
		buf = chip->data_buf;
#else
		buf = chip->buffers->databuf;
#endif
		/* Invalidate page cache */
		chip->pagebuf = -1;
	}

	/* read without ecc for verification */
	ret = chip->ecc.read_page_raw(mtd, chip, buf, true, page);
	if (ret)
		return ret;

	for (i = 0; i < chip->ecc.steps; i++) {
		ecc_chunk = buf + chip->ecc.size * i;

		mtd_ooblayout_ecc(mtd, i, &ecc);
		ecc_bytes = chip->oob_poi + ecc.offset;

		ret = nand_check_erased_ecc_chunk(ecc_chunk, chip->ecc.size,
						  ecc_bytes, ecc.length,
						  NULL, 0,
						  chip->ecc.strength);
		if (ret < 0)
			return ret;

		bitflips = max(bitflips, ret);
	}

	return bitflips;
}

static int nfsnand_read(struct mtd_info *mtd, struct nand_chip *chip,
			 u64 addr, unsigned int trans, u32 *buf, u8 *oob)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;
	u64 err_addr = 0;
	int err;
	bool retry = true;

	dev_dbg(ctrl->dev, "read %llx -> %p\n", (unsigned long long)addr, buf);

try_dmaread:
	nfsnand_clear_ecc_addr(ctrl);

#ifndef __UBOOT__
	if (has_flash_dma(ctrl) && !oob && flash_dma_buf_ok(buf)) {
		err = nfsnand_dma_trans(host, addr, buf, trans * FC_BYTES,
					     CMD_PAGE_READ);
		if (err) {
			if (mtd_is_bitflip_or_eccerr(err))
				err_addr = addr;
			else
				return -EIO;
		}
	} else {
		if (oob)
			memset(oob, 0x99, mtd->oobsize);

		err = nfsnand_read_by_pio(mtd, chip, addr, trans, buf,
					       oob, &err_addr);
	}
#else
	if (oob)
		memset(oob, 0x99, mtd->oobsize);

	err = nfsnand_read_by_pio(mtd, chip, addr, trans, buf,
							   oob, &err_addr);
#endif /* __UBOOT__ */

	if (mtd_is_eccerr(err)) {
		/*
		 * On controller version and 7.0, 7.1 , DMA read after a
		 * prior PIO read that reported uncorrectable error,
		 * the DMA engine captures this error following DMA read
		 * cleared only on subsequent DMA read, so just retry once
		 * to clear a possible false error reported for current DMA
		 * read
		 */
		if ((ctrl->nand_version == 0x0700) ||
		    (ctrl->nand_version == 0x0701)) {
			if (retry) {
				retry = false;
				goto try_dmaread;
			}
		}

		/*
		 * Controller version 7.2 has hw encoder to detect erased page
		 * bitflips, apply sw verification for older controllers only
		 */
		if (ctrl->nand_version < 0x0702) {
			err = nfsstb_nand_verify_erased_page(mtd, chip, buf,
							      addr);
			/* erased page bitflips corrected */
			if (err >= 0)
				return err;
		}

		dev_dbg(ctrl->dev, "uncorrectable error at 0x%llx\n",
			(unsigned long long)err_addr);
		mtd->ecc_stats.failed++;
		/* NAND layer expects zero on ECC errors */
		return 0;
	}

	if (mtd_is_bitflip(err)) {
		unsigned int corrected = nfsnand_count_corrected(ctrl);

		dev_dbg(ctrl->dev, "corrected error at 0x%llx\n",
			(unsigned long long)err_addr);
		mtd->ecc_stats.corrected += corrected;
		/* Always exceed the software-imposed threshold */
		return max(mtd->bitflip_threshold, corrected);
	}

	return 0;
}

static int nfsnand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int oob_required, int page)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;

	nand_read_page_op(chip, page, 0, NULL, 0);

	return nfsnand_read(mtd, chip, host->last_addr,
			mtd->writesize >> FC_SHIFT, (u32 *)buf, oob);
}

static int nfsnand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				  uint8_t *buf, int oob_required, int page)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;
	int ret;

	nand_read_page_op(chip, page, 0, NULL, 0);

	nfsnand_set_ecc_enabled(host, 0);
	ret = nfsnand_read(mtd, chip, host->last_addr,
			mtd->writesize >> FC_SHIFT, (u32 *)buf, oob);
	nfsnand_set_ecc_enabled(host, 1);
	return ret;
}

static int nfsnand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			     int page)
{
	return nfsnand_read(mtd, chip, (u64)page << chip->page_shift,
			mtd->writesize >> FC_SHIFT,
			NULL, (u8 *)chip->oob_poi);
}

static int nfsnand_read_oob_raw(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);

	nfsnand_set_ecc_enabled(host, 0);
	nfsnand_read(mtd, chip, (u64)page << chip->page_shift,
		mtd->writesize >> FC_SHIFT,
		NULL, (u8 *)chip->oob_poi);
	nfsnand_set_ecc_enabled(host, 1);
	return 0;
}

static int nfsnand_write(struct mtd_info *mtd, struct nand_chip *chip,
			  u64 addr, const u32 *buf, u8 *oob)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	struct nfsnand_controller *ctrl = host->ctrl;
	unsigned int i, j, trans = mtd->writesize >> FC_SHIFT;
	int status, ret = 0;

	dev_dbg(ctrl->dev, "write %llx <- %p\n", (unsigned long long)addr, buf);

	if (unlikely((unsigned long)buf & 0x03)) {
		dev_warn(ctrl->dev, "unaligned buffer: %p\n", buf);
		buf = (u32 *)((unsigned long)buf & ~0x03);
	}

	nfsnand_wp(mtd, 0);

	for (i = 0; i < ctrl->max_oob; i += 4)
		oob_reg_write(ctrl, i, 0xffffffff);

#ifndef __UBOOT__
	if (has_flash_dma(ctrl) && !oob && flash_dma_buf_ok(buf)) {
		if (nfsnand_dma_trans(host, addr, (u32 *)buf,
					mtd->writesize, CMD_PROGRAM_PAGE))
			ret = -EIO;
		goto out;
	}
#endif /* __UBOOT__ */

	for (i = 0; i < trans; i++, addr += FC_BYTES) {
		/* full address MUST be set before populating FC */
		nfsnand_set_cmd_addr(mtd, addr);

		if (buf) {
			nfsnand_soc_data_bus_prepare(ctrl->soc, false);

			for (j = 0; j < FC_WORDS; j++, buf++)
				nfsnand_write_fc(ctrl, j, *buf);

			nfsnand_soc_data_bus_unprepare(ctrl->soc, false);
		} else if (oob) {
			for (j = 0; j < FC_WORDS; j++)
				nfsnand_write_fc(ctrl, j, 0xffffffff);
		}

		if (oob) {
			oob += write_oob_to_regs(ctrl, i, oob,
					mtd->oobsize / trans,
					host->hwcfg.sector_size_1k);
		}

		/* we cannot use SPARE_AREA_PROGRAM when PARTIAL_PAGE_EN=0 */
		nfsnand_send_cmd(host, CMD_PROGRAM_PAGE);
		status = nfsnand_waitfunc(mtd, chip);

		if (status & NAND_STATUS_FAIL) {
			dev_info(ctrl->dev, "program failed at %llx\n",
				(unsigned long long)addr);
			ret = -EIO;
			goto out;
		}
	}
out:
	nfsnand_wp(mtd, 1);
	return ret;
}

static int nfsnand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			       const uint8_t *buf, int oob_required, int page)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	void *oob = oob_required ? chip->oob_poi : NULL;

	nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	nfsnand_write(mtd, chip, host->last_addr, (const u32 *)buf, oob);

	return nand_prog_page_end_op(chip);
}

static int nfsnand_write_page_raw(struct mtd_info *mtd,
				   struct nand_chip *chip, const uint8_t *buf,
				   int oob_required, int page)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	void *oob = oob_required ? chip->oob_poi : NULL;

	nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	nfsnand_set_ecc_enabled(host, 0);
	nfsnand_write(mtd, chip, host->last_addr, (const u32 *)buf, oob);
	nfsnand_set_ecc_enabled(host, 1);

	return nand_prog_page_end_op(chip);
}

static int nfsnand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				  int page)
{
	return nfsnand_write(mtd, chip, (u64)page << chip->page_shift,
				  NULL, chip->oob_poi);
}

static int nfsnand_write_oob_raw(struct mtd_info *mtd, struct nand_chip *chip,
				  int page)
{
	struct nfsnand_host *host = nand_get_controller_data(chip);
	int ret;

	nfsnand_set_ecc_enabled(host, 0);
	ret = nfsnand_write(mtd, chip, (u64)page << chip->page_shift, NULL,
				 (u8 *)chip->oob_poi);
	nfsnand_set_ecc_enabled(host, 1);

	return ret;
}

/***********************************************************************
 * Per-CS setup (1 NAND device)
 ***********************************************************************/

static int nfsnand_set_cfg(struct nfsnand_host *host,
			    struct nfsnand_cfg *cfg)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	struct nand_chip *chip = &host->chip;
	u16 cfg_offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_CFG);
	u16 cfg_ext_offs = nfsnand_cs_offset(ctrl, host->cs,
			NFSNAND_CS_CFG_EXT);
	u16 acc_control_offs = nfsnand_cs_offset(ctrl, host->cs,
			NFSNAND_CS_ACC_CONTROL);
	u8 block_size = 0, page_size = 0, device_size = 0;
	u32 tmp;

	if (ctrl->block_sizes) {
		int i, found;

		for (i = 0, found = 0; ctrl->block_sizes[i]; i++)
			if (ctrl->block_sizes[i] * 1024 == cfg->block_size) {
				block_size = i;
				found = 1;
			}
		if (!found) {
			dev_warn(ctrl->dev, "invalid block size %u\n",
					cfg->block_size);
			return -EINVAL;
		}
	} else {
		block_size = ffs(cfg->block_size) - ffs(NFSNAND_MIN_BLOCKSIZE);
	}

	if (cfg->block_size < NFSNAND_MIN_BLOCKSIZE || (ctrl->max_block_size &&
				cfg->block_size > ctrl->max_block_size)) {
		dev_warn(ctrl->dev, "invalid block size %u\n",
				cfg->block_size);
		block_size = 0;
	}

	if (ctrl->page_sizes) {
		int i, found;

		for (i = 0, found = 0; ctrl->page_sizes[i]; i++)
			if (ctrl->page_sizes[i] == cfg->page_size) {
				page_size = i;
				found = 1;
			}
		if (!found) {
			dev_warn(ctrl->dev, "invalid page size %u\n",
					cfg->page_size);
			return -EINVAL;
		}
	} else {
		page_size = ffs(cfg->page_size) - ffs(NFSNAND_MIN_PAGESIZE);
	}

	if (cfg->page_size < NFSNAND_MIN_PAGESIZE || (ctrl->max_page_size &&
				cfg->page_size > ctrl->max_page_size)) {
		dev_warn(ctrl->dev, "invalid page size %u\n", cfg->page_size);
		return -EINVAL;
	}

	if (fls64(cfg->device_size) < fls64(NFSNAND_MIN_DEVSIZE)) {
		dev_warn(ctrl->dev, "invalid device size 0x%llx\n",
			(unsigned long long)cfg->device_size);
		return -EINVAL;
	}
	device_size = fls64(cfg->device_size) - fls64(NFSNAND_MIN_DEVSIZE);

	tmp = (cfg->blk_adr_bytes << CFG_BLK_ADR_BYTES_SHIFT) |
		(cfg->col_adr_bytes << CFG_COL_ADR_BYTES_SHIFT) |
		(cfg->ful_adr_bytes << CFG_FUL_ADR_BYTES_SHIFT) |
		(!!(cfg->device_width == 16) << CFG_BUS_WIDTH_SHIFT) |
		(device_size << CFG_DEVICE_SIZE_SHIFT);
	if (cfg_offs == cfg_ext_offs) {
		tmp |= (page_size << ctrl->page_size_shift) |
		       (block_size << CFG_BLK_SIZE_SHIFT);
		nand_writereg(ctrl, cfg_offs, tmp);
	} else {
		nand_writereg(ctrl, cfg_offs, tmp);
		tmp = (page_size << CFG_EXT_PAGE_SIZE_SHIFT) |
		      (block_size << CFG_EXT_BLK_SIZE_SHIFT);
		nand_writereg(ctrl, cfg_ext_offs, tmp);
	}

	tmp = nand_readreg(ctrl, acc_control_offs);
	tmp &= ~nfsnand_ecc_level_mask(ctrl);
	tmp &= ~nfsnand_spare_area_mask(ctrl);
	if (ctrl->nand_version >= 0x0302) {
		tmp |= cfg->ecc_level << NAND_ACC_CONTROL_ECC_SHIFT;
		tmp |= cfg->spare_area_size;
	}
	nand_writereg(ctrl, acc_control_offs, tmp);

	nfsnand_set_sector_size_1k(host, cfg->sector_size_1k);

	/* threshold = ceil(BCH-level * 0.75) */
	nfsnand_wr_corr_thresh(host, DIV_ROUND_UP(chip->ecc.strength * 3, 4));

	return 0;
}

static void nfsnand_print_cfg(struct nfsnand_host *host,
			       char *buf, struct nfsnand_cfg *cfg)
{
	buf += sprintf(buf,
		"%lluMiB total, %uKiB blocks, %u%s pages, %uB OOB, %u-bit",
		(unsigned long long)cfg->device_size >> 20,
		cfg->block_size >> 10,
		cfg->page_size >= 1024 ? cfg->page_size >> 10 : cfg->page_size,
		cfg->page_size >= 1024 ? "KiB" : "B",
		cfg->spare_area_size, cfg->device_width);

	/* Account for Hamming ECC and for BCH 512B vs 1KiB sectors */
	if (is_hamming_ecc(host->ctrl, cfg))
		sprintf(buf, ", Hamming ECC");
	else if (cfg->sector_size_1k)
		sprintf(buf, ", BCH-%u (1KiB sector)", cfg->ecc_level << 1);
	else
		sprintf(buf, ", BCH-%u", cfg->ecc_level);
}

/*
 * Minimum number of bytes to address a page. Calculated as:
 *     roundup(log2(size / page-size) / 8)
 *
 * NB: the following does not "round up" for non-power-of-2 'size'; but this is
 *     OK because many other things will break if 'size' is irregular...
 */
static inline int get_blk_adr_bytes(u64 size, u32 writesize)
{
	return ALIGN(ilog2(size) - ilog2(writesize), 8) >> 3;
}

static int nfsnand_setup_dev(struct nfsnand_host *host)
{
	struct mtd_info *mtd = nand_to_mtd(&host->chip);
	struct nand_chip *chip = &host->chip;
	struct nfsnand_controller *ctrl = host->ctrl;
	struct nfsnand_cfg *cfg = &host->hwcfg;
	char msg[128];
	u32 offs, tmp, oob_sector;
	int ret;

	memset(cfg, 0, sizeof(*cfg));

#ifndef __UBOOT__
	ret = of_property_read_u32(nand_get_flash_node(chip),
				   "nfs,nand-oob-sector-size",
				   &oob_sector);
#else
	ret = ofnode_read_u32(nand_get_flash_node(chip),
			      "nfs,nand-oob-sector-size",
			      &oob_sector);
#endif /* __UBOOT__ */
	if (ret) {
		/* Use detected size */
		cfg->spare_area_size = mtd->oobsize /
					(mtd->writesize >> FC_SHIFT);
	} else {
		cfg->spare_area_size = oob_sector;
	}
	if (cfg->spare_area_size > ctrl->max_oob)
		cfg->spare_area_size = ctrl->max_oob;
	/*
	 * Set oobsize to be consistent with controller's spare_area_size, as
	 * the rest is inaccessible.
	 */
	mtd->oobsize = cfg->spare_area_size * (mtd->writesize >> FC_SHIFT);

	cfg->device_size = mtd->size;
	cfg->block_size = mtd->erasesize;
	cfg->page_size = mtd->writesize;
	cfg->device_width = (chip->options & NAND_BUSWIDTH_16) ? 16 : 8;
	cfg->col_adr_bytes = 2;
	cfg->blk_adr_bytes = get_blk_adr_bytes(mtd->size, mtd->writesize);

	if (chip->ecc.mode != NAND_ECC_HW) {
		dev_err(ctrl->dev, "only HW ECC supported; selected: %d\n",
			chip->ecc.mode);
		return -EINVAL;
	}

	if (chip->ecc.algo == NAND_ECC_UNKNOWN) {
		if (chip->ecc.strength == 1 && chip->ecc.size == 512)
			/* Default to Hamming for 1-bit ECC, if unspecified */
			chip->ecc.algo = NAND_ECC_HAMMING;
		else
			/* Otherwise, BCH */
			chip->ecc.algo = NAND_ECC_BCH;
	}

	if (chip->ecc.algo == NAND_ECC_HAMMING && (chip->ecc.strength != 1 ||
						   chip->ecc.size != 512)) {
		dev_err(ctrl->dev, "invalid Hamming params: %d bits per %d bytes\n",
			chip->ecc.strength, chip->ecc.size);
		return -EINVAL;
	}

	switch (chip->ecc.size) {
	case 512:
		if (chip->ecc.algo == NAND_ECC_HAMMING)
			cfg->ecc_level = 15;
		else
			cfg->ecc_level = chip->ecc.strength;
		cfg->sector_size_1k = 0;
		break;
	case 1024:
		if (!(ctrl->features & NFSNAND_HAS_1K_SECTORS)) {
			dev_err(ctrl->dev, "1KB sectors not supported\n");
			return -EINVAL;
		}
		if (chip->ecc.strength & 0x1) {
			dev_err(ctrl->dev,
				"odd ECC not supported with 1KB sectors\n");
			return -EINVAL;
		}

		cfg->ecc_level = chip->ecc.strength >> 1;
		cfg->sector_size_1k = 1;
		break;
	default:
		dev_err(ctrl->dev, "unsupported ECC size: %d\n",
			chip->ecc.size);
		return -EINVAL;
	}

	cfg->ful_adr_bytes = cfg->blk_adr_bytes;
	if (mtd->writesize > 512)
		cfg->ful_adr_bytes += cfg->col_adr_bytes;
	else
		cfg->ful_adr_bytes += 1;

	ret = nfsnand_set_cfg(host, cfg);
	if (ret)
		return ret;

	nfsnand_set_ecc_enabled(host, 1);

	nfsnand_print_cfg(host, msg, cfg);
	dev_info(ctrl->dev, "detected %s\n", msg);

	/* Configure ACC_CONTROL */
	offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_ACC_CONTROL);
	tmp = nand_readreg(ctrl, offs);
	tmp &= ~ACC_CONTROL_PARTIAL_PAGE;
	tmp &= ~ACC_CONTROL_RD_ERASED;

	/* We need to turn on Read from erased paged protected by ECC */
	if (ctrl->nand_version >= 0x0702)
		tmp |= ACC_CONTROL_RD_ERASED;
	tmp &= ~ACC_CONTROL_FAST_PGM_RDIN;
	if (ctrl->features & NFSNAND_HAS_PREFETCH)
		tmp &= ~ACC_CONTROL_PREFETCH;

	nand_writereg(ctrl, offs, tmp);

	return 0;
}

#ifndef __UBOOT__
static int nfsnand_init_cs(struct nfsnand_host *host, struct device_node *dn)
#else
static int nfsnand_init_cs(struct nfsnand_host *host, ofnode dn)
#endif
{
	struct nfsnand_controller *ctrl = host->ctrl;
#ifndef __UBOOT__
	struct platform_device *pdev = host->pdev;
#else
	struct udevice *pdev = host->pdev;
#endif /* __UBOOT__ */
	struct mtd_info *mtd;
	struct nand_chip *chip;
	int ret;
	u16 cfg_offs;

#ifndef __UBOOT__
	ret = of_property_read_u32(dn, "reg", &host->cs);
#else
	ret = ofnode_read_s32(dn, "reg", &host->cs);
#endif
	if (ret) {
		dev_err(pdev, "can't get chip-select\n");
		return -ENXIO;
	}

	mtd = nand_to_mtd(&host->chip);
	chip = &host->chip;

	nand_set_flash_node(chip, dn);
	nand_set_controller_data(chip, host);
#ifndef __UBOOT__
	mtd->name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "nfsnand.%d",
				   host->cs);
#else
	mtd->name = devm_kasprintf(pdev, GFP_KERNEL, "nfsnand.%d",
				   host->cs);
#endif /* __UBOOT__ */
	if (!mtd->name)
		return -ENOMEM;

	mtd->owner = THIS_MODULE;
#ifndef __UBOOT__
	mtd->dev.parent = &pdev->dev;
#else
	mtd->dev->parent = pdev;
#endif /* __UBOOT__ */

	chip->IO_ADDR_R = (void __iomem *)0xdeadbeef;
	chip->IO_ADDR_W = (void __iomem *)0xdeadbeef;

	chip->cmd_ctrl = nfsnand_cmd_ctrl;
	chip->cmdfunc = nfsnand_cmdfunc;
	chip->waitfunc = nfsnand_waitfunc;
	chip->read_byte = nfsnand_read_byte;
	chip->read_buf = nfsnand_read_buf;
	chip->write_buf = nfsnand_write_buf;

	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.read_page = nfsnand_read_page;
	chip->ecc.write_page = nfsnand_write_page;
	chip->ecc.read_page_raw = nfsnand_read_page_raw;
	chip->ecc.write_page_raw = nfsnand_write_page_raw;
	chip->ecc.write_oob_raw = nfsnand_write_oob_raw;
	chip->ecc.read_oob_raw = nfsnand_read_oob_raw;
	chip->ecc.read_oob = nfsnand_read_oob;
	chip->ecc.write_oob = nfsnand_write_oob;

	chip->controller = &ctrl->controller;

	/*
	 * The bootloader might have configured 16bit mode but
	 * NAND READID command only works in 8bit mode. We force
	 * 8bit mode here to ensure that NAND READID commands works.
	 */
	cfg_offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_CFG);
	nand_writereg(ctrl, cfg_offs,
		      nand_readreg(ctrl, cfg_offs) & ~CFG_BUS_WIDTH);

	ret = nand_scan_ident(mtd, 1, NULL);
	if (ret)
		return ret;

	chip->options |= NAND_NO_SUBPAGE_WRITE;
	/*
	 * Avoid (for instance) kmap()'d buffers from JFFS2, which we can't DMA
	 * to/from, and have nand_base pass us a bounce buffer instead, as
	 * needed.
	 */
	chip->options |= NAND_USE_BOUNCE_BUFFER;

	if (chip->bbt_options & NAND_BBT_USE_FLASH)
		chip->bbt_options |= NAND_BBT_NO_OOB;

	if (nfsnand_setup_dev(host))
		return -ENXIO;

	chip->ecc.size = host->hwcfg.sector_size_1k ? 1024 : 512;
	/* only use our internal HW threshold */
	mtd->bitflip_threshold = 1;

	chip->ecc.layout = nfsstb_choose_ecc_layout(host);
	if (!chip->ecc.layout)
		return -ENXIO;

	ret = nand_scan_tail(mtd);
	if (ret)
		return ret;

#ifndef __UBOOT__
	ret = mtd_device_register(mtd, NULL, 0);
	if (ret)
		nand_cleanup(chip);
#else
	ret = nand_register(0, mtd);
#endif /* __UBOOT__ */

	/* If OOB is written with ECC enabled it will cause ECC errors */
	if (is_hamming_ecc(host->ctrl, &host->hwcfg)) {
		chip->ecc.write_oob = nfsnand_write_oob_raw;
		chip->ecc.read_oob = nfsnand_read_oob_raw;
	}

	return ret;
}

#ifndef __UBOOT__
static void nfsnand_save_restore_cs_config(struct nfsnand_host *host,
					    int restore)
{
	struct nfsnand_controller *ctrl = host->ctrl;
	u16 cfg_offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_CFG);
	u16 cfg_ext_offs = nfsnand_cs_offset(ctrl, host->cs,
			NFSNAND_CS_CFG_EXT);
	u16 acc_control_offs = nfsnand_cs_offset(ctrl, host->cs,
			NFSNAND_CS_ACC_CONTROL);
	u16 t1_offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_TIMING1);
	u16 t2_offs = nfsnand_cs_offset(ctrl, host->cs, NFSNAND_CS_TIMING2);

	if (restore) {
		nand_writereg(ctrl, cfg_offs, host->hwcfg.config);
		if (cfg_offs != cfg_ext_offs)
			nand_writereg(ctrl, cfg_ext_offs,
				      host->hwcfg.config_ext);
		nand_writereg(ctrl, acc_control_offs, host->hwcfg.acc_control);
		nand_writereg(ctrl, t1_offs, host->hwcfg.timing_1);
		nand_writereg(ctrl, t2_offs, host->hwcfg.timing_2);
	} else {
		host->hwcfg.config = nand_readreg(ctrl, cfg_offs);
		if (cfg_offs != cfg_ext_offs)
			host->hwcfg.config_ext =
				nand_readreg(ctrl, cfg_ext_offs);
		host->hwcfg.acc_control = nand_readreg(ctrl, acc_control_offs);
		host->hwcfg.timing_1 = nand_readreg(ctrl, t1_offs);
		host->hwcfg.timing_2 = nand_readreg(ctrl, t2_offs);
	}
}

static int nfsnand_suspend(struct device *dev)
{
	struct nfsnand_controller *ctrl = dev_get_drvdata(dev);
	struct nfsnand_host *host;

	list_for_each_entry(host, &ctrl->host_list, node)
		nfsnand_save_restore_cs_config(host, 0);

	ctrl->nand_cs_nand_select = nfsnand_read_reg(ctrl, NFSNAND_CS_SELECT);
	ctrl->nand_cs_nand_xor = nfsnand_read_reg(ctrl, NFSNAND_CS_XOR);
	ctrl->corr_stat_threshold =
		nfsnand_read_reg(ctrl, NFSNAND_CORR_THRESHOLD);

	if (has_flash_dma(ctrl))
		ctrl->flash_dma_mode = flash_dma_readl(ctrl, FLASH_DMA_MODE);

	return 0;
}

static int nfsnand_resume(struct device *dev)
{
	struct nfsnand_controller *ctrl = dev_get_drvdata(dev);
	struct nfsnand_host *host;

	if (has_flash_dma(ctrl)) {
		flash_dma_writel(ctrl, FLASH_DMA_MODE, ctrl->flash_dma_mode);
		flash_dma_writel(ctrl, FLASH_DMA_ERROR_STATUS, 0);
	}

	nfsnand_write_reg(ctrl, NFSNAND_CS_SELECT, ctrl->nand_cs_nand_select);
	nfsnand_write_reg(ctrl, NFSNAND_CS_XOR, ctrl->nand_cs_nand_xor);
	nfsnand_write_reg(ctrl, NFSNAND_CORR_THRESHOLD,
			ctrl->corr_stat_threshold);
	if (ctrl->soc) {
		/* Clear/re-enable interrupt */
		ctrl->soc->ctlrdy_ack(ctrl->soc);
		ctrl->soc->ctlrdy_set_enabled(ctrl->soc, true);
	}

	list_for_each_entry(host, &ctrl->host_list, node) {
		struct nand_chip *chip = &host->chip;

		nfsnand_save_restore_cs_config(host, 1);

		/* Reset the chip, required by some chips after power-up */
		nand_reset_op(chip);
	}

	return 0;
}

const struct dev_pm_ops nfsnand_pm_ops = {
	.suspend		= nfsnand_suspend,
	.resume			= nfsnand_resume,
};
EXPORT_SYMBOL_GPL(nfsnand_pm_ops);

static const struct of_device_id nfsnand_of_match[] = {
	{ .compatible = "nfs,nfsnand-v2.1" },
	{ .compatible = "nfs,nfsnand-v2.2" },
	{ .compatible = "nfs,nfsnand-v4.0" },
	{ .compatible = "nfs,nfsnand-v5.0" },
	{ .compatible = "nfs,nfsnand-v6.0" },
	{ .compatible = "nfs,nfsnand-v6.1" },
	{ .compatible = "nfs,nfsnand-v6.2" },
	{ .compatible = "nfs,nfsnand-v7.0" },
	{ .compatible = "nfs,nfsnand-v7.1" },
	{ .compatible = "nfs,nfsnand-v7.2" },
	{ .compatible = "nfs,nfsnand-v7.3" },
	{},
};
MODULE_DEVICE_TABLE(of, nfsnand_of_match);
#endif  /* __UBOOT__ */

/***********************************************************************
 * Platform driver setup (per controller)
 ***********************************************************************/

#ifndef __UBOOT__
int nfsnand_probe(struct platform_device *pdev, struct nfsnand_soc *soc)
#else
int nfsnand_probe(struct udevice *dev, struct nfsnand_soc *soc)
#endif /* __UBOOT__ */
{
#ifndef __UBOOT__
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node, *child;
#else
	ofnode child;
	struct udevice *pdev = dev;
#endif /* __UBOOT__ */
	struct nfsnand_controller *ctrl;
#ifndef __UBOOT__
	struct resource *res;
#else
	struct resource res;
#endif /* __UBOOT__ */
	int ret;

#ifndef __UBOOT__
	/* We only support device-tree instantiation */
	if (!dn)
		return -ENODEV;

	if (!of_match_node(nfsnand_of_match, dn))
		return -ENODEV;
#endif /* __UBOOT__ */

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

#ifndef __UBOOT__
	dev_set_drvdata(dev, ctrl);
#else
	/*
	 * in u-boot, the data for the driver is allocated before probing
	 * so to keep the reference to ctrl, we store it in the variable soc
	 */
	soc->ctrl = ctrl;
#endif /* __UBOOT__ */
	ctrl->dev = dev;

	init_completion(&ctrl->done);
	init_completion(&ctrl->dma_done);
	nand_hw_control_init(&ctrl->controller);
	INIT_LIST_HEAD(&ctrl->host_list);

	/* Is parameter page in big endian ? */
	ctrl->parameter_page_big_endian =
	    dev_read_u32_default(dev, "parameter-page-big-endian", 1);

	/* NAND register range */
#ifndef __UBOOT__
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctrl->nand_base = devm_ioremap_resource(dev, res);
#else
	dev_read_resource(pdev, 0, &res);
	ctrl->nand_base = devm_ioremap(pdev, res.start, resource_size(&res));
#endif
	if (IS_ERR(ctrl->nand_base))
		return PTR_ERR(ctrl->nand_base);

	/* Enable clock before using NAND registers */
	ctrl->clk = devm_clk_get(dev, "nand");
	if (!IS_ERR(ctrl->clk)) {
		ret = clk_prepare_enable(ctrl->clk);
		if (ret)
			return ret;
	} else {
		/* Ignore PTR_ERR(ctrl->clk) */
		ctrl->clk = NULL;
	}

	/* Initialize NAND revision */
	ret = nfsnand_revision_init(ctrl);
	if (ret)
		goto err;

	/*
	 * Most chips have this cache at a fixed offset within 'nand' block.
	 * Some must specify this region separately.
	 */
#ifndef __UBOOT__
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nand-cache");
	if (res) {
		ctrl->nand_fc = devm_ioremap_resource(dev, res);
		if (IS_ERR(ctrl->nand_fc)) {
			ret = PTR_ERR(ctrl->nand_fc);
			goto err;
		}
	} else {
		ctrl->nand_fc = ctrl->nand_base +
				ctrl->reg_offsets[NFSNAND_FC_BASE];
	}
#else
	if (!dev_read_resource_byname(pdev, "nand-cache", &res)) {
		ctrl->nand_fc = devm_ioremap(dev, res.start,
					     resource_size(&res));
		if (IS_ERR(ctrl->nand_fc)) {
			ret = PTR_ERR(ctrl->nand_fc);
			goto err;
		}
	} else {
		ctrl->nand_fc = ctrl->nand_base +
				ctrl->reg_offsets[NFSNAND_FC_BASE];
	}
#endif

#ifndef __UBOOT__
	/* FLASH_DMA */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "flash-dma");
	if (res) {
		ctrl->flash_dma_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(ctrl->flash_dma_base)) {
			ret = PTR_ERR(ctrl->flash_dma_base);
			goto err;
		}

		/* initialize the dma version */
		nfsnand_flash_dma_revision_init(ctrl);

		/* linked-list and stop on error */
		flash_dma_writel(ctrl, FLASH_DMA_MODE, FLASH_DMA_MODE_MASK);
		flash_dma_writel(ctrl, FLASH_DMA_ERROR_STATUS, 0);

		/* Allocate descriptor(s) */
		ctrl->dma_desc = dmam_alloc_coherent(dev,
						     sizeof(*ctrl->dma_desc),
						     &ctrl->dma_pa, GFP_KERNEL);
		if (!ctrl->dma_desc) {
			ret = -ENOMEM;
			goto err;
		}

		ctrl->dma_irq = platform_get_irq(pdev, 1);
		if ((int)ctrl->dma_irq < 0) {
			dev_err(dev, "missing FLASH_DMA IRQ\n");
			ret = -ENODEV;
			goto err;
		}

		ret = devm_request_irq(dev, ctrl->dma_irq,
				nfsnand_dma_irq, 0, DRV_NAME,
				ctrl);
		if (ret < 0) {
			dev_err(dev, "can't allocate IRQ %d: error %d\n",
					ctrl->dma_irq, ret);
			goto err;
		}

		dev_info(dev, "enabling FLASH_DMA\n");
	}
#endif /* __UBOOT__ */

	/* Disable automatic device ID config, direct addressing */
	nfsnand_rmw_reg(ctrl, NFSNAND_CS_SELECT,
			 CS_SELECT_AUTO_DEVICE_ID_CFG | 0xff, 0, 0);
	/* Disable XOR addressing */
	nfsnand_rmw_reg(ctrl, NFSNAND_CS_XOR, 0xff, 0, 0);

	/* Read the write-protect configuration in the device tree */
	wp_on = dev_read_u32_default(dev, "write-protect", wp_on);

	if (ctrl->features & NFSNAND_HAS_WP) {
		/* Permanently disable write protection */
		if (wp_on == 2)
			nfsnand_set_wp(ctrl, false);
	} else {
		wp_on = 0;
	}

#ifndef __UBOOT__
	/* IRQ */
	ctrl->irq = platform_get_irq(pdev, 0);
	if ((int)ctrl->irq < 0) {
		dev_err(dev, "no IRQ defined\n");
		ret = -ENODEV;
		goto err;
	}

	/*
	 * Some SoCs integrate this controller (e.g., its interrupt bits) in
	 * interesting ways
	 */
	if (soc) {
		ctrl->soc = soc;

		ret = devm_request_irq(dev, ctrl->irq, nfsnand_irq, 0,
				       DRV_NAME, ctrl);

		/* Enable interrupt */
		ctrl->soc->ctlrdy_ack(ctrl->soc);
		ctrl->soc->ctlrdy_set_enabled(ctrl->soc, true);
	} else {
		/* Use standard interrupt infrastructure */
		ret = devm_request_irq(dev, ctrl->irq, nfsnand_ctlrdy_irq, 0,
				       DRV_NAME, ctrl);
	}
	if (ret < 0) {
		dev_err(dev, "can't allocate IRQ %d: error %d\n",
			ctrl->irq, ret);
		goto err;
	}
#endif /* __UBOOT__ */

#ifndef __UBOOT__
	for_each_available_child_of_node(dn, child) {
		if (of_device_is_compatible(child, "nfs,nandcs")) {
			struct nfsnand_host *host;

			host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
			if (!host) {
				of_node_put(child);
				ret = -ENOMEM;
				goto err;
			}
			host->pdev = pdev;
			host->ctrl = ctrl;

			ret = nfsnand_init_cs(host, child);
			if (ret) {
				devm_kfree(dev, host);
				continue; /* Try all chip-selects */
			}

			list_add_tail(&host->node, &ctrl->host_list);
		}
	}
#else
	ofnode_for_each_subnode(child, dev_ofnode(dev)) {
		if (ofnode_device_is_compatible(child, "nfs,nandcs")) {
			struct nfsnand_host *host;

			host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
			if (!host) {
				ret = -ENOMEM;
				goto err;
			}
			host->pdev = pdev;
			host->ctrl = ctrl;

			ret = nfsnand_init_cs(host, child);
			if (ret) {
				devm_kfree(dev, host);
				continue; /* Try all chip-selects */
			}

			list_add_tail(&host->node, &ctrl->host_list);
		}
	}
#endif /* __UBOOT__ */

	/* No chip-selects could initialize properly */
	if (list_empty(&ctrl->host_list)) {
		ret = -ENODEV;
		goto err;
	}

	return 0;

err:
#ifndef __UBOOT__
	clk_disable_unprepare(ctrl->clk);
#else
	if (ctrl->clk)
		clk_disable(ctrl->clk);
#endif /* __UBOOT__ */
	return ret;
}
EXPORT_SYMBOL_GPL(nfsnand_probe);

#ifndef __UBOOT__
int nfsnand_remove(struct platform_device *pdev)
{
	struct nfsnand_controller *ctrl = dev_get_drvdata(&pdev->dev);
	struct nfsnand_host *host;

	list_for_each_entry(host, &ctrl->host_list, node)
		nand_release(nand_to_mtd(&host->chip));

	clk_disable_unprepare(ctrl->clk);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}
#else
int nfsnand_remove(struct udevice *pdev)
{
	return 0;
}
#endif /* __UBOOT__ */
EXPORT_SYMBOL_GPL(nfsnand_remove);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Kevin Cernekee");
MODULE_AUTHOR("Brian Norris");
MODULE_DESCRIPTION("NAND driver for Broadcom chips");
MODULE_ALIAS("platform:nfsnand");
