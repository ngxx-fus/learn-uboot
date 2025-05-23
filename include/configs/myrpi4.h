/* include/configs/myrpi4.h */
#ifndef __CONFIG_MYRPI4CP_H
#define __CONFIG_MYRPI4CP_H

#pragma message ("The .h file is compiled!")


#define CONFIG_SYS_CONFIG_NAME "myrpi4"
#define CONFIG_BOARDDIR board/ngxxfus/myrpi4

/* Disable distro boot to avoid BOOTENV conflicts */
#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
    "mmc dev 0; " \
    "fatload mmc 0:1 ${kernel_addr_r} Image; " \
    "fatload mmc 0:1 ${fdt_addr_r} bcm2711-rpi-4-b.dtb; " \
    "bootm ${kernel_addr_r} - ${fdt_addr_r}"

#endif /* __CONFIG_MYRPI4CP_H */