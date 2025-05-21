cmd_arch/arm/lib/relocate_64.o := aarch64-linux-gnu-gcc -Wp,-MD,arch/arm/lib/.relocate_64.o.d -nostdinc -isystem /usr/lib/gcc-cross/aarch64-linux-gnu/13/include -Iinclude   -I./arch/arm/include -include ./include/linux/kconfig.h -D__KERNEL__ -D__UBOOT__ -D__ASSEMBLY__ -fno-PIE -gdwarf-4 -D__ARM__ -fno-pic -mstrict-align -ffunction-sections -fdata-sections -fno-common -ffixed-x18 -mgeneral-regs-only -mbranch-protection=none -pipe -march=armv8-a+crc -D__LINUX_ARM_ARCH__=8   -c -o arch/arm/lib/relocate_64.o arch/arm/lib/relocate_64.S

source_arch/arm/lib/relocate_64.o := arch/arm/lib/relocate_64.S

deps_arch/arm/lib/relocate_64.o := \
    $(wildcard include/config/text/base.h) \
  include/linux/kconfig.h \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
    $(wildcard include/config/spl/.h) \
    $(wildcard include/config/tpl/.h) \
    $(wildcard include/config/tools/.h) \
    $(wildcard include/config/tpl/build.h) \
    $(wildcard include/config/vpl/build.h) \
    $(wildcard include/config/spl/build.h) \
    $(wildcard include/config/tools/foo.h) \
    $(wildcard include/config/spl/foo.h) \
    $(wildcard include/config/tpl/foo.h) \
    $(wildcard include/config/vpl/foo.h) \
    $(wildcard include/config/option.h) \
    $(wildcard include/config/acme.h) \
    $(wildcard include/config/spl/acme.h) \
    $(wildcard include/config/tpl/acme.h) \
    $(wildcard include/config/if/enabled/int.h) \
    $(wildcard include/config/int/option.h) \
  include/asm-offsets.h \
  include/generated/generic-asm-offsets.h \
  include/config.h \
  include/configs/myrpi4cp.h \
    $(wildcard include/config/show/debug/message.h) \
    $(wildcard include/config/arm64.h) \
    $(wildcard include/config/cmd/dfu.h) \
    $(wildcard include/config/auto/zreladdr.h) \
    $(wildcard include/config/cmd/mmc.h) \
    $(wildcard include/config/cmd/usb.h) \
    $(wildcard include/config/cmd/pxe.h) \
    $(wildcard include/config/cmd/dhcp.h) \
  include/linux/sizes.h \
  include/linux/const.h \
  arch/arm/include/asm/arch/timer.h \
  include/config_distro_bootcmd.h \
    $(wildcard include/config/sandbox.h) \
    $(wildcard include/config/cmd/ubifs.h) \
    $(wildcard include/config/efi/loader.h) \
    $(wildcard include/config/arm.h) \
    $(wildcard include/config/x86/run/32bit.h) \
    $(wildcard include/config/x86/run/64bit.h) \
    $(wildcard include/config/arch/rv32i.h) \
    $(wildcard include/config/arch/rv64i.h) \
    $(wildcard include/config/cmd/bootefi/bootmgr.h) \
    $(wildcard include/config/sata.h) \
    $(wildcard include/config/nvme.h) \
    $(wildcard include/config/scsi.h) \
    $(wildcard include/config/ide.h) \
    $(wildcard include/config/pci.h) \
    $(wildcard include/config/cmd/virtio.h) \
    $(wildcard include/config/x86.h) \
    $(wildcard include/config/cmd/extension.h) \
  arch/arm/include/asm/config.h \
    $(wildcard include/config/arch/ls1021a.h) \
    $(wildcard include/config/fsl/layerscape.h) \
  include/linux/kconfig.h \
  include/config_fallbacks.h \
    $(wildcard include/config/spl/pad/to.h) \
    $(wildcard include/config/spl/max/size.h) \
  include/elf.h \
  include/linux/linkage.h \
  arch/arm/include/asm/linkage.h \
  arch/arm/include/asm/macro.h \
    $(wildcard include/config/armv8/multientry.h) \
    $(wildcard include/config/armv8/psci.h) \
    $(wildcard include/config/armv8/ea/el3/first.h) \
    $(wildcard include/config/gicv3.h) \
    $(wildcard include/config/gicv2.h) \
  arch/arm/include/asm/system.h \
    $(wildcard include/config/armv7/lpae.h) \
    $(wildcard include/config/cpu/v7a.h) \
    $(wildcard include/config/sys/arm/cache/writethrough.h) \
    $(wildcard include/config/sys/arm/cache/writealloc.h) \
    $(wildcard include/config/sys/arm/cache/writeback.h) \
    $(wildcard include/config/armv7/psci.h) \
    $(wildcard include/config/sys/noncached/memory.h) \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/kasan.h) \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/optimize/inlining.h) \
    $(wildcard include/config/cc/has/asm/inline.h) \
  arch/arm/include/asm/barriers.h \

arch/arm/lib/relocate_64.o: $(deps_arch/arm/lib/relocate_64.o)

$(deps_arch/arm/lib/relocate_64.o):
