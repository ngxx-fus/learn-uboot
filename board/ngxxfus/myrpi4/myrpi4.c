#include <common.h>
#include <dm.h>
#include <mmc.h>
#include <asm/io.h>
#include <asm/arch/sd_emmc.h>
#include <asm/arch/mbox.h>
#include <asm/arch/sdhci.h>
#include <asm/gpio.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <linux/sizes.h>

#pragma message ("The .c file is compiled!")

DECLARE_GLOBAL_DATA_PTR;

/* Define pad control settings for UART and SDHC (not needed for RPi 4 as pins are pre-configured by VideoCore) */
#define UART_PAD_CTRL  0 /* Placeholder - not used */
#define USDHC_PAD_CTRL 0 /* Placeholder - not used */

/* DRAM initialization: RPi 4 DRAM size is set by VideoCore, query via mailbox */
int dram_init(void)
{
    unsigned long ram_size;

    /* Use mailbox to query RAM size from VideoCore firmware */
    ram_size = bcm2835_mbox_get_arm_memory();
    if (ram_size == 0) {
        /* Fallback to a default size if mailbox fails */
        ram_size = SZ_1G; /* Default to 1GB if detection fails */
    }

    gd->ram_size = ram_size;
    return 0;
}

/* Board-specific initialization */
int board_init(void)
{
    printf("[board_init] Started ... ");
    
    /* Address of boot parameters */
    gd->bd->bi_boot_params = CONFIG_SYS_TEXT_BASE + 0x100;
    
    printf("[board_init] Done!!!!!");
    return 0;
}

/* SDHC configuration for RPi 4 */
static struct sdhci_host sdhc_host = {
    .name = "bcm2835_sdhc",
    .ioaddr = (void *)SDHCI_BASE_ADDR,
    .bus_width = 4,
};

/* Card detection (CD) pin - RPi 4 does not have a dedicated CD pin, assume card is always present */
int board_mmc_getcd(struct mmc *mmc)
{
    /* RPi 4 does not have a CD pin; assume card is always present */
    return 1;
}

/* MMC initialization for RPi 4 */
int board_mmc_init(bd_t *bis)
{
    struct mmc *mmc;
    int ret;

    /* Initialize SDHC controller for RPi 4 */
    sdhc_host.host_caps = MMC_MODE_4BIT | MMC_MODE_HS_52MHz;
    ret = sdhci_setup_cfg(&sdhc_host);
    if (ret) {
        printf("Failed to setup SDHC: %d\n", ret);
        return ret;
    }

    /* Initialize MMC device */
    mmc = mmc_create(&sdhc_host, NULL);
    if (!mmc) {
        printf("Failed to create MMC device\n");
        return -1;
    }

    /* Add MMC device to U-Boot */
    ret = mmc_init(mmc);
    if (ret) {
        printf("Failed to initialize MMC: %d\n", ret);
        return ret;
    }

    return 0;
}

/* Identify the board */
int checkboard(void)
{
    puts("Board: NGXXFUS Raspberry Pi 4\n");
    return 0;
}

/* UART setup (not needed as VideoCore firmware configures UART pins) */
static void setup_iomux_uart(void)
{
    /* UART pins (GPIO 14/15 for TX/RX) are already configured by VideoCore */
}

/* Early board initialization */
int board_early_init_f(void)
{
    setup_iomux_uart();
    return 0;
}

/* Main board initialization (non-SPL path) */
void board_init_f(ulong dummy)
{
    /* Disable watchdog (not applicable for RPi 4) */
    arch_cpu_init();

    /* Early board init */
    board_early_init_f();

    /* Setup timer */
    timer_init();

    /* Initialize serial console */
    preloader_console_init();

    /* DRAM initialization */
    dram_init();

    /* Clear the BSS section */
    memset(__bss_start, 0, __bss_end - __bss_start);

    /* Proceed to main U-Boot loop */
    board_init_r(NULL, 0);
}