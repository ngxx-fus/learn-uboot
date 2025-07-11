// SPDX-License-Identifier: GPL-2.0
/*
 * nf2835 sdhost driver.
 *
 * The 2835 has two SD controllers: The Arasan sdhci controller
 * (supported by the iproc driver) and a custom sdhost controller
 * (supported by this driver).
 *
 * The sdhci controller supports both sdcard and sdio.  The sdhost
 * controller supports the sdcard only, but has better performance.
 * Also note that the rpi3 has sdio wifi, so driving the sdcard with
 * the sdhost controller allows to use the sdhci controller for wifi
 * support.
 *
 * The configuration is done by devicetree via pin muxing.  Both
 * SD controller are available on the same pins (2 pin groups = pin 22
 * to 27 + pin 48 to 53).  So it's possible to use both SD controllers
 * at the same time with different pin groups.
 *
 * This code was ported to U-Boot by
 *  Alexander Graf <agraf@suse.de>
 * and is based on drivers/mmc/host/nf2835.c in Linux which is written by
 *  Phil Elwell <phil@raspberrypi.org>
 *  Copyright (C) 2015-2016 Raspberry Pi (Trading) Ltd.
 * which is based on
 *  mmc-nf2835.c by Gellert Weisz
 * which is, in turn, based on
 *  sdhci-nf2708.c by Broadcom
 *  sdhci-nf2835.c by Stephen Warren and Oleksandr Tymoshenko
 *  sdhci.c and sdhci-pci.c by Pierre Ossman
 */


#include <clk.h>
#include <common.h>
#include <dm.h>
#include <mmc.h>
#include <asm/arch/msg.h>
#include <asm/arch/mbox.h>
#include <asm/unaligned.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#include <mach/gpio.h>
#include <power/regulator.h>

#define msleep(a) udelay(a * 1000)

#define SDCMD  0x00 /* Command to SD card              - 16 R/W */
#define SDARG  0x04 /* Argument to SD card             - 32 R/W */
#define SDTOUT 0x08 /* Start value for timeout counter - 32 R/W */
#define SDCDIV 0x0c /* Start value for clock divider   - 11 R/W */
#define SDRSP0 0x10 /* SD card response (31:0)         - 32 R   */
#define SDRSP1 0x14 /* SD card response (63:32)        - 32 R   */
#define SDRSP2 0x18 /* SD card response (95:64)        - 32 R   */
#define SDRSP3 0x1c /* SD card response (127:96)       - 32 R   */
#define SDHSTS 0x20 /* SD host status                  - 11 R/W */
#define SDVDD  0x30 /* SD card power control           -  1 R/W */
#define SDEDM  0x34 /* Emergency Debug Mode            - 13 R/W */
#define SDHCFG 0x38 /* Host configuration              -  2 R/W */
#define SDHBCT 0x3c /* Host byte count (debug)         - 32 R/W */
#define SDDATA 0x40 /* Data to/from SD card            - 32 R/W */
#define SDHBLC 0x50 /* Host block count (SDIO/SDHC)    -  9 R/W */

#define SDCMD_NEW_FLAG			0x8000
#define SDCMD_FAIL_FLAG			0x4000
#define SDCMD_BUSYWAIT			0x800
#define SDCMD_NO_RESPONSE		0x400
#define SDCMD_LONG_RESPONSE		0x200
#define SDCMD_WRITE_CMD			0x80
#define SDCMD_READ_CMD			0x40
#define SDCMD_CMD_MASK			0x3f

#define SDCDIV_MAX_CDIV			0x7ff

#define SDHSTS_BUSY_IRPT		0x400
#define SDHSTS_BLOCK_IRPT		0x200
#define SDHSTS_SDIO_IRPT		0x100
#define SDHSTS_REW_TIME_OUT		0x80
#define SDHSTS_CMD_TIME_OUT		0x40
#define SDHSTS_CRC16_ERROR		0x20
#define SDHSTS_CRC7_ERROR		0x10
#define SDHSTS_FIFO_ERROR		0x08
#define SDHSTS_DATA_FLAG		0x01

#define SDHSTS_CLEAR_MASK		(SDHSTS_BUSY_IRPT | \
					 SDHSTS_BLOCK_IRPT | \
					 SDHSTS_SDIO_IRPT | \
					 SDHSTS_REW_TIME_OUT | \
					 SDHSTS_CMD_TIME_OUT | \
					 SDHSTS_CRC16_ERROR | \
					 SDHSTS_CRC7_ERROR | \
					 SDHSTS_FIFO_ERROR)

#define SDHSTS_TRANSFER_ERROR_MASK	(SDHSTS_CRC7_ERROR | \
					 SDHSTS_CRC16_ERROR | \
					 SDHSTS_REW_TIME_OUT | \
					 SDHSTS_FIFO_ERROR)

#define SDHSTS_ERROR_MASK		(SDHSTS_CMD_TIME_OUT | \
					 SDHSTS_TRANSFER_ERROR_MASK)

#define SDHCFG_BUSY_IRPT_EN	BIT(10)
#define SDHCFG_BLOCK_IRPT_EN	BIT(8)
#define SDHCFG_SDIO_IRPT_EN	BIT(5)
#define SDHCFG_DATA_IRPT_EN	BIT(4)
#define SDHCFG_SLOW_CARD	BIT(3)
#define SDHCFG_WIDE_EXT_BUS	BIT(2)
#define SDHCFG_WIDE_INT_BUS	BIT(1)
#define SDHCFG_REL_CMD_LINE	BIT(0)

#define SDVDD_POWER_OFF		0
#define SDVDD_POWER_ON		1

#define SDEDM_FORCE_DATA_MODE	BIT(19)
#define SDEDM_CLOCK_PULSE	BIT(20)
#define SDEDM_BYPASS		BIT(21)

#define SDEDM_FIFO_FILL_SHIFT	4
#define SDEDM_FIFO_FILL_MASK	0x1f
static u32 edm_fifo_fill(u32 edm)
{
	return (edm >> SDEDM_FIFO_FILL_SHIFT) & SDEDM_FIFO_FILL_MASK;
}

#define SDEDM_WRITE_THRESHOLD_SHIFT	9
#define SDEDM_READ_THRESHOLD_SHIFT	14
#define SDEDM_THRESHOLD_MASK		0x1f

#define SDEDM_FSM_MASK		0xf
#define SDEDM_FSM_IDENTMODE	0x0
#define SDEDM_FSM_DATAMODE	0x1
#define SDEDM_FSM_READDATA	0x2
#define SDEDM_FSM_WRITEDATA	0x3
#define SDEDM_FSM_READWAIT	0x4
#define SDEDM_FSM_READCRC	0x5
#define SDEDM_FSM_WRITECRC	0x6
#define SDEDM_FSM_WRITEWAIT1	0x7
#define SDEDM_FSM_POWERDOWN	0x8
#define SDEDM_FSM_POWERUP	0x9
#define SDEDM_FSM_WRITESTART1	0xa
#define SDEDM_FSM_WRITESTART2	0xb
#define SDEDM_FSM_GENPULSES	0xc
#define SDEDM_FSM_WRITEWAIT2	0xd
#define SDEDM_FSM_STARTPOWDOWN	0xf

#define SDDATA_FIFO_WORDS	16

#define FIFO_READ_THRESHOLD	4
#define FIFO_WRITE_THRESHOLD	4
#define SDDATA_FIFO_PIO_BURST	8

#define SDHST_TIMEOUT_MAX_USEC	100000

struct nf2835_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct nf2835_host {
	void __iomem		*ioaddr;
	u32			phys_addr;

	int			clock;		/* Current clock speed */
	unsigned int		max_clk;	/* Max possible freq */
	unsigned int		blocks;		/* remaining PIO blocks */

	u32			ns_per_fifo_word;

	/* cached registers */
	u32			hcfg;
	u32			cdiv;

	struct mmc_cmd	*cmd;		/* Current command */
	struct mmc_data		*data;		/* Current data request */
	bool			use_busy:1;	/* Wait for busy interrupt */

	struct udevice		*dev;
	struct mmc		*mmc;
	struct nf2835_plat	*plat;
	unsigned int		firmware_sets_cdiv:1;
};

static void nf2835_dumpregs(struct nf2835_host *host)
{
	dev_dbg(host->dev, "=========== REGISTER DUMP ===========\n");
	dev_dbg(host->dev, "SDCMD  0x%08x\n", readl(host->ioaddr + SDCMD));
	dev_dbg(host->dev, "SDARG  0x%08x\n", readl(host->ioaddr + SDARG));
	dev_dbg(host->dev, "SDTOUT 0x%08x\n", readl(host->ioaddr + SDTOUT));
	dev_dbg(host->dev, "SDCDIV 0x%08x\n", readl(host->ioaddr + SDCDIV));
	dev_dbg(host->dev, "SDRSP0 0x%08x\n", readl(host->ioaddr + SDRSP0));
	dev_dbg(host->dev, "SDRSP1 0x%08x\n", readl(host->ioaddr + SDRSP1));
	dev_dbg(host->dev, "SDRSP2 0x%08x\n", readl(host->ioaddr + SDRSP2));
	dev_dbg(host->dev, "SDRSP3 0x%08x\n", readl(host->ioaddr + SDRSP3));
	dev_dbg(host->dev, "SDHSTS 0x%08x\n", readl(host->ioaddr + SDHSTS));
	dev_dbg(host->dev, "SDVDD  0x%08x\n", readl(host->ioaddr + SDVDD));
	dev_dbg(host->dev, "SDEDM  0x%08x\n", readl(host->ioaddr + SDEDM));
	dev_dbg(host->dev, "SDHCFG 0x%08x\n", readl(host->ioaddr + SDHCFG));
	dev_dbg(host->dev, "SDHBCT 0x%08x\n", readl(host->ioaddr + SDHBCT));
	dev_dbg(host->dev, "SDHBLC 0x%08x\n", readl(host->ioaddr + SDHBLC));
	dev_dbg(host->dev, "===========================================\n");
}

static void nf2835_reset_internal(struct nf2835_host *host)
{
	u32 temp;

	writel(SDVDD_POWER_OFF, host->ioaddr + SDVDD);
	writel(0, host->ioaddr + SDCMD);
	writel(0, host->ioaddr + SDARG);
	/* Set timeout to a big enough value so we don't hit it */
	writel(0xf00000, host->ioaddr + SDTOUT);
	writel(0, host->ioaddr + SDCDIV);
	/* Clear status register */
	writel(SDHSTS_CLEAR_MASK, host->ioaddr + SDHSTS);
	writel(0, host->ioaddr + SDHCFG);
	writel(0, host->ioaddr + SDHBCT);
	writel(0, host->ioaddr + SDHBLC);

	/* Limit fifo usage due to silicon bug */
	temp = readl(host->ioaddr + SDEDM);
	temp &= ~((SDEDM_THRESHOLD_MASK << SDEDM_READ_THRESHOLD_SHIFT) |
		  (SDEDM_THRESHOLD_MASK << SDEDM_WRITE_THRESHOLD_SHIFT));
	temp |= (FIFO_READ_THRESHOLD << SDEDM_READ_THRESHOLD_SHIFT) |
		(FIFO_WRITE_THRESHOLD << SDEDM_WRITE_THRESHOLD_SHIFT);
	writel(temp, host->ioaddr + SDEDM);
	/* Wait for FIFO threshold to populate */
	msleep(20);
	writel(SDVDD_POWER_ON, host->ioaddr + SDVDD);
	/* Wait for all components to go through power on cycle */
	msleep(20);
	host->clock = 0;
	writel(host->hcfg, host->ioaddr + SDHCFG);
	writel(SDCDIV_MAX_CDIV, host->ioaddr + SDCDIV);
}

static int nf2835_wait_transfer_complete(struct nf2835_host *host)
{
	ulong tstart_ms = get_timer(0);

	while (1) {
		u32 edm, fsm;

		edm = readl(host->ioaddr + SDEDM);
		fsm = edm & SDEDM_FSM_MASK;

		if ((fsm == SDEDM_FSM_IDENTMODE) ||
		    (fsm == SDEDM_FSM_DATAMODE))
			break;

		if ((fsm == SDEDM_FSM_READWAIT) ||
		    (fsm == SDEDM_FSM_WRITESTART1) ||
		    (fsm == SDEDM_FSM_READDATA)) {
			writel(edm | SDEDM_FORCE_DATA_MODE,
			       host->ioaddr + SDEDM);
			break;
		}

		/* Error out after ~1s */
		ulong tlapse_ms = get_timer(tstart_ms);
		if ( tlapse_ms > 1000 /* ms */ ) {

			dev_err(host->dev,
				"wait_transfer_complete - still waiting after %lu ms\n",
				tlapse_ms);
			nf2835_dumpregs(host);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int nf2835_transfer_block_pio(struct nf2835_host *host, bool is_read)
{
	struct mmc_data *data = host->data;
	size_t blksize = data->blocksize;
	int copy_words;
	u32 hsts = 0;
	u32 *buf;

	if (blksize % sizeof(u32))
		return -EINVAL;

	buf = is_read ? (u32 *)data->dest : (u32 *)data->src;

	if (is_read)
		data->dest += blksize;
	else
		data->src += blksize;

	copy_words = blksize / sizeof(u32);

	/*
	 * Copy all contents from/to the FIFO as far as it reaches,
	 * then wait for it to fill/empty again and rewind.
	 */
	while (copy_words) {
		int burst_words, words;
		u32 edm;

		burst_words = min(SDDATA_FIFO_PIO_BURST, copy_words);
		edm = readl(host->ioaddr + SDEDM);
		if (is_read)
			words = edm_fifo_fill(edm);
		else
			words = SDDATA_FIFO_WORDS - edm_fifo_fill(edm);

		if (words < burst_words) {
			int fsm_state = (edm & SDEDM_FSM_MASK);

			if ((is_read &&
			     (fsm_state != SDEDM_FSM_READDATA &&
			      fsm_state != SDEDM_FSM_READWAIT &&
			      fsm_state != SDEDM_FSM_READCRC)) ||
			    (!is_read &&
			     (fsm_state != SDEDM_FSM_WRITEDATA &&
			      fsm_state != SDEDM_FSM_WRITEWAIT1 &&
			      fsm_state != SDEDM_FSM_WRITEWAIT2 &&
			      fsm_state != SDEDM_FSM_WRITECRC &&
			      fsm_state != SDEDM_FSM_WRITESTART1 &&
			      fsm_state != SDEDM_FSM_WRITESTART2))) {
				hsts = readl(host->ioaddr + SDHSTS);
				printf("fsm %x, hsts %08x\n", fsm_state, hsts);
				if (hsts & SDHSTS_ERROR_MASK)
					break;
			}

			continue;
		} else if (words > copy_words) {
			words = copy_words;
		}

		copy_words -= words;

		/* Copy current chunk to/from the FIFO */
		while (words) {
			if (is_read)
				*(buf++) = readl(host->ioaddr + SDDATA);
			else
				writel(*(buf++), host->ioaddr + SDDATA);
			words--;
		}
	}

	return 0;
}

static int nf2835_transfer_pio(struct nf2835_host *host)
{
	u32 sdhsts;
	bool is_read;
	int ret = 0;

	is_read = (host->data->flags & MMC_DATA_READ) != 0;
	ret = nf2835_transfer_block_pio(host, is_read);
	if (ret)
		return ret;

	sdhsts = readl(host->ioaddr + SDHSTS);
	if (sdhsts & (SDHSTS_CRC16_ERROR |
		      SDHSTS_CRC7_ERROR |
		      SDHSTS_FIFO_ERROR)) {
		printf("%s transfer error - HSTS %08x\n",
		       is_read ? "read" : "write", sdhsts);
		ret =  -EILSEQ;
	} else if ((sdhsts & (SDHSTS_CMD_TIME_OUT |
			      SDHSTS_REW_TIME_OUT))) {
		printf("%s timeout error - HSTS %08x\n",
		       is_read ? "read" : "write", sdhsts);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static void nf2835_prepare_data(struct nf2835_host *host, struct mmc_cmd *cmd,
				 struct mmc_data *data)
{
	WARN_ON(host->data);

	host->data = data;
	if (!data)
		return;

	/* Use PIO */
	host->blocks = data->blocks;

	writel(data->blocksize, host->ioaddr + SDHBCT);
	writel(data->blocks, host->ioaddr + SDHBLC);
}

static u32 nf2835_read_wait_sdcmd(struct nf2835_host *host)
{
	u32 value;
	int ret;
	int timeout_us = SDHST_TIMEOUT_MAX_USEC;

	ret = readl_poll_timeout(host->ioaddr + SDCMD, value,
				 !(value & SDCMD_NEW_FLAG), timeout_us);
	if (ret == -ETIMEDOUT)
		printf("%s: timeout (%d us)\n", __func__, timeout_us);

	return value;
}

static int nf2835_send_command(struct nf2835_host *host, struct mmc_cmd *cmd,
				struct mmc_data *data)
{
	u32 sdcmd, sdhsts;

	WARN_ON(host->cmd);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY)) {
		printf("unsupported response type!\n");
		return -EINVAL;
	}

	sdcmd = nf2835_read_wait_sdcmd(host);
	if (sdcmd & SDCMD_NEW_FLAG) {
		printf("previous command never completed.\n");
		nf2835_dumpregs(host);
		return -EBUSY;
	}

	host->cmd = cmd;

	/* Clear any error flags */
	sdhsts = readl(host->ioaddr + SDHSTS);
	if (sdhsts & SDHSTS_ERROR_MASK)
		writel(sdhsts, host->ioaddr + SDHSTS);

	nf2835_prepare_data(host, cmd, data);

	writel(cmd->cmdarg, host->ioaddr + SDARG);

	sdcmd = cmd->cmdidx & SDCMD_CMD_MASK;

	host->use_busy = false;
	if (!(cmd->resp_type & MMC_RSP_PRESENT)) {
		sdcmd |= SDCMD_NO_RESPONSE;
	} else {
		if (cmd->resp_type & MMC_RSP_136)
			sdcmd |= SDCMD_LONG_RESPONSE;
		if (cmd->resp_type & MMC_RSP_BUSY) {
			sdcmd |= SDCMD_BUSYWAIT;
			host->use_busy = true;
		}
	}

	if (data) {
		if (data->flags & MMC_DATA_WRITE)
			sdcmd |= SDCMD_WRITE_CMD;
		if (data->flags & MMC_DATA_READ)
			sdcmd |= SDCMD_READ_CMD;
	}

	writel(sdcmd | SDCMD_NEW_FLAG, host->ioaddr + SDCMD);

	return 0;
}

static int nf2835_finish_command(struct nf2835_host *host)
{
	struct mmc_cmd *cmd = host->cmd;
	u32 sdcmd;
	int ret = 0;

	sdcmd = nf2835_read_wait_sdcmd(host);

	/* Check for errors */
	if (sdcmd & SDCMD_NEW_FLAG) {
		printf("command never completed.\n");
		nf2835_dumpregs(host);
		return -EIO;
	} else if (sdcmd & SDCMD_FAIL_FLAG) {
		u32 sdhsts = readl(host->ioaddr + SDHSTS);

		/* Clear the errors */
		writel(SDHSTS_ERROR_MASK, host->ioaddr + SDHSTS);

		if (!(sdhsts & SDHSTS_CRC7_ERROR) ||
		    (host->cmd->cmdidx != MMC_CMD_SEND_OP_COND)) {
			if (sdhsts & SDHSTS_CMD_TIME_OUT) {
				ret = -ETIMEDOUT;
			} else {
				printf("unexpected command %d error\n",
				       host->cmd->cmdidx);
				nf2835_dumpregs(host);
				ret = -EILSEQ;
			}

			return ret;
		}
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			int i;

			for (i = 0; i < 4; i++) {
				cmd->response[3 - i] =
					readl(host->ioaddr + SDRSP0 + i * 4);
			}
		} else {
			cmd->response[0] = readl(host->ioaddr + SDRSP0);
		}
	}

	/* Processed actual command. */
	host->cmd = NULL;

	return ret;
}

static int nf2835_check_cmd_error(struct nf2835_host *host, u32 intmask)
{
	int ret = -EINVAL;

	if (!(intmask & SDHSTS_ERROR_MASK))
		return 0;

	if (!host->cmd)
		return -EINVAL;

	printf("sdhost_busy_irq: intmask %08x\n", intmask);
	if (intmask & SDHSTS_CRC7_ERROR) {
		ret = -EILSEQ;
	} else if (intmask & (SDHSTS_CRC16_ERROR |
			      SDHSTS_FIFO_ERROR)) {
		ret = -EILSEQ;
	} else if (intmask & (SDHSTS_REW_TIME_OUT | SDHSTS_CMD_TIME_OUT)) {
		ret = -ETIMEDOUT;
	}
	nf2835_dumpregs(host);
	return ret;
}

static int nf2835_check_data_error(struct nf2835_host *host, u32 intmask)
{
	int ret = 0;

	if (!host->data)
		return 0;
	if (intmask & (SDHSTS_CRC16_ERROR | SDHSTS_FIFO_ERROR))
		ret = -EILSEQ;
	if (intmask & SDHSTS_REW_TIME_OUT)
		ret = -ETIMEDOUT;

	if (ret)
		printf("%s:%d %d\n", __func__, __LINE__, ret);

	return ret;
}

static int nf2835_transmit(struct nf2835_host *host)
{
	u32 intmask = readl(host->ioaddr + SDHSTS);
	int ret;

	/* Check for errors */
	ret = nf2835_check_data_error(host, intmask);
	if (ret)
		return ret;

	ret = nf2835_check_cmd_error(host, intmask);
	if (ret)
		return ret;

	/* Handle wait for busy end */
	if (host->use_busy && (intmask & SDHSTS_BUSY_IRPT)) {
		writel(SDHSTS_BUSY_IRPT, host->ioaddr + SDHSTS);
		host->use_busy = false;
		nf2835_finish_command(host);
	}

	/* Handle PIO data transfer */
	if (host->data) {
		ret = nf2835_transfer_pio(host);
		if (ret)
			return ret;
		host->blocks--;
		if (host->blocks == 0) {
			/* Wait for command to complete for real */
			ret = nf2835_wait_transfer_complete(host);
			if (ret)
				return ret;
			/* Transfer complete */
			host->data = NULL;
		}
	}

	return 0;
}

static void nf2835_set_clock(struct nf2835_host *host, unsigned int clock)
{
	int div;
	u32 clock_rate[2] = { 0 };

	/* The SDCDIV register has 11 bits, and holds (div - 2).  But
	 * in data mode the max is 50MHz wihout a minimum, and only
	 * the bottom 3 bits are used. Since the switch over is
	 * automatic (unless we have marked the card as slow...),
	 * chosen values have to make sense in both modes.  Ident mode
	 * must be 100-400KHz, so can range check the requested
	 * clock. CMD15 must be used to return to data mode, so this
	 * can be monitored.
	 *
	 * clock 250MHz -> 0->125MHz, 1->83.3MHz, 2->62.5MHz, 3->50.0MHz
	 *                 4->41.7MHz, 5->35.7MHz, 6->31.3MHz, 7->27.8MHz
	 *
	 *		 623->400KHz/27.8MHz
	 *		 reset value (507)->491159/50MHz
	 *
	 * BUT, the 3-bit clock divisor in data mode is too small if
	 * the core clock is higher than 250MHz, so instead use the
	 * SLOW_CARD configuration bit to force the use of the ident
	 * clock divisor at all times.
	 */

	if (host->firmware_sets_cdiv) {
		nf2835_set_sdhost_clock(clock, &clock_rate[0], &clock_rate[1]);
		clock = max(clock_rate[0], clock_rate[1]);
	} else {
		if (clock < 100000) {
			/* Can't stop the clock, but make it as slow as possible
			* to show willing
			*/
			host->cdiv = SDCDIV_MAX_CDIV;
			writel(host->cdiv, host->ioaddr + SDCDIV);
			return;
		}

		div = host->max_clk / clock;
		if (div < 2)
			div = 2;
		if ((host->max_clk / div) > clock)
			div++;
		div -= 2;

		if (div > SDCDIV_MAX_CDIV)
			div = SDCDIV_MAX_CDIV;

		clock = host->max_clk / (div + 2);
		host->cdiv = div;
		writel(host->cdiv, host->ioaddr + SDCDIV);
	}

	host->mmc->clock = clock;

	/* Calibrate some delays */

	host->ns_per_fifo_word = (1000000000 / clock) *
		((host->mmc->card_caps & MMC_MODE_4BIT) ? 8 : 32);

	/* Set the timeout to 500ms */
	writel(host->mmc->clock / 2, host->ioaddr + SDTOUT);
}

static inline int is_power_of_2(u64 x)
{
	return !(x & (x - 1));
}

static int nf2835_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			    struct mmc_data *data)
{
	struct nf2835_host *host = dev_get_priv(dev);
	u32 edm, fsm;
	int ret = 0;

	if (data && !is_power_of_2(data->blocksize)) {
		printf("unsupported block size (%d bytes)\n", data->blocksize);

		if (cmd)
			return -EINVAL;
	}

	edm = readl(host->ioaddr + SDEDM);
	fsm = edm & SDEDM_FSM_MASK;

	if ((fsm != SDEDM_FSM_IDENTMODE) &&
	    (fsm != SDEDM_FSM_DATAMODE) &&
	    (cmd && cmd->cmdidx != MMC_CMD_STOP_TRANSMISSION)) {
		printf("previous command (%d) not complete (EDM %08x)\n",
		       readl(host->ioaddr + SDCMD) & SDCMD_CMD_MASK, edm);
		nf2835_dumpregs(host);

		if (cmd)
			return -EILSEQ;

		return 0;
	}

	if (cmd) {
		ret = nf2835_send_command(host, cmd, data);
		if (!ret && !host->use_busy)
			ret = nf2835_finish_command(host);
	}

	/* Wait for completion of busy signal or data transfer */
	while (host->use_busy || host->data) {
		ret = nf2835_transmit(host);
		if (ret)
			break;
	}

	return ret;
}

static int nf2835_set_ios(struct udevice *dev)
{
	struct nf2835_host *host = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);

	if (!mmc->clock || mmc->clock != host->clock) {
		nf2835_set_clock(host, mmc->clock);
		host->clock = mmc->clock;
	}

	/* set bus width */
	host->hcfg &= ~SDHCFG_WIDE_EXT_BUS;
	if (mmc->bus_width == 4)
		host->hcfg |= SDHCFG_WIDE_EXT_BUS;

	host->hcfg |= SDHCFG_WIDE_INT_BUS;

	/* Disable clever clock switching, to cope with fast core clocks */
	host->hcfg |= SDHCFG_SLOW_CARD;

	writel(host->hcfg, host->ioaddr + SDHCFG);

	return 0;
}

static void nf2835_add_host(struct nf2835_host *host)
{
	struct mmc_config *cfg = &host->plat->cfg;

	cfg->f_max = host->max_clk;
	cfg->f_min = host->max_clk / SDCDIV_MAX_CDIV;
	cfg->b_max = 65535;

	dev_dbg(host->dev, "f_max %d, f_min %d\n",
		cfg->f_max, cfg->f_min);

	/* host controller capabilities */
	cfg->host_caps = MMC_MODE_4BIT | MMC_MODE_HS | MMC_MODE_HS_52MHz;

	/* report supported voltage ranges */
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;

	/* Set interrupt enables */
	host->hcfg = SDHCFG_BUSY_IRPT_EN;

	nf2835_reset_internal(host);
}

static int nf2835_probe(struct udevice *dev)
{
	struct nf2835_plat *plat = dev_get_plat(dev);
	struct nf2835_host *host = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	u32 clock_rate[2] = { ~0 };

	host->dev = dev;
	host->mmc = mmc;
	host->plat = plat;
	upriv->mmc = &plat->mmc;
	plat->cfg.name = dev->name;

	host->phys_addr = dev_read_addr(dev);
	if (host->phys_addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	host->ioaddr = devm_ioremap(dev, host->phys_addr, SZ_256);
	if (!host->ioaddr)
		return -ENOMEM;

	host->max_clk = nf2835_get_mmc_clock(NF2835_MBOX_CLOCK_ID_CORE);

	nf2835_set_sdhost_clock(0, &clock_rate[0], &clock_rate[1]);
	host->firmware_sets_cdiv = (clock_rate[0] != ~0);

	nf2835_add_host(host);

	dev_dbg(dev, "%s -> OK\n", __func__);

	return 0;
}

static const struct udevice_id nf2835_match[] = {
	{ .compatible = "nfs,nf2835-sdhost" },
	{ }
};

static const struct dm_mmc_ops nf2835_ops = {
	.send_cmd = nf2835_send_cmd,
	.set_ios = nf2835_set_ios,
};

static int nf2835_bind(struct udevice *dev)
{
	struct nf2835_plat *plat = dev_get_plat(dev);

	return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

U_BOOT_DRIVER(nf2835_sdhost) = {
	.name = "nf2835-sdhost",
	.id = UCLASS_MMC,
	.of_match = nf2835_match,
	.bind = nf2835_bind,
	.probe = nf2835_probe,
	.priv_auto	= sizeof(struct nf2835_host),
	.plat_auto	= sizeof(struct nf2835_plat),
	.ops = &nf2835_ops,
};
