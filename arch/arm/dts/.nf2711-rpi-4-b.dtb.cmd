cmd_arch/arm/dts/nf2711-rpi-4-b.dtb := mkdir -p arch/arm/dts/ ; (cat arch/arm/dts/nf2711-rpi-4-b.dts; echo '$(pound)include "nf271x-u-boot.dtsi"') > arch/arm/dts/.nf2711-rpi-4-b.dtb.pre.tmp;  cc -E -Wp,-MD,arch/arm/dts/.nf2711-rpi-4-b.dtb.d.pre.tmp -nostdinc -Iinclude   -I./arch/arm/include -include ./include/linux/kconfig.h -I./arch/arm/dts -I./arch/arm/dts/include -I./include -D__ASSEMBLY__ -undef -D__DTS__ -x assembler-with-cpp -o arch/arm/dts/.nf2711-rpi-4-b.dtb.dts.tmp arch/arm/dts/.nf2711-rpi-4-b.dtb.pre.tmp ; ./scripts/dtc/dtc -O dtb -o arch/arm/dts/nf2711-rpi-4-b.dtb -b 0 -i arch/arm/dts/ -Wno-unit_address_vs_reg -Wno-unit_address_format -Wno-avoid_unnecessary_addr_size -Wno-alias_paths -Wno-graph_child_address -Wno-graph_port -Wno-unique_unit_address -Wno-simple_bus_reg -Wno-pci_device_reg -Wno-pci_bridge -Wno-pci_device_bus_num  -@ -a 0x8 -Wno-unit_address_vs_reg -Wno-unit_address_format -Wno-avoid_unnecessary_addr_size -Wno-alias_paths -Wno-graph_child_address -Wno-graph_port -Wno-unique_unit_address -Wno-simple_bus_reg -Wno-pci_device_reg -Wno-pci_bridge -Wno-pci_device_bus_num  -@ -d arch/arm/dts/.nf2711-rpi-4-b.dtb.d.dtc.tmp arch/arm/dts/.nf2711-rpi-4-b.dtb.dts.tmp || (echo "Check /home/ngxxfus/Desktop/mod_uboot/arch/arm/dts/.nf2711-rpi-4-b.dtb.pre.tmp for errors" && false) ; sed "s:arch/arm/dts/.nf2711-rpi-4-b.dtb.pre.tmp:arch/arm/dts/nf2711-rpi-4-b.dts:" arch/arm/dts/.nf2711-rpi-4-b.dtb.d.pre.tmp arch/arm/dts/.nf2711-rpi-4-b.dtb.d.dtc.tmp > arch/arm/dts/.nf2711-rpi-4-b.dtb.d

source_arch/arm/dts/nf2711-rpi-4-b.dtb := arch/arm/dts/nf2711-rpi-4-b.dts

deps_arch/arm/dts/nf2711-rpi-4-b.dtb := \
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
  arch/arm/dts/bcm2711.dtsi \
  arch/arm/dts/bcm283x.dtsi \
  include/dt-bindings/pinctrl/bcm2835.h \
  include/dt-bindings/clock/bcm2835.h \
  include/dt-bindings/clock/bcm2835-aux.h \
  include/dt-bindings/gpio/gpio.h \
  include/dt-bindings/interrupt-controller/irq.h \
  include/dt-bindings/soc/bcm2835-pm.h \
  include/dt-bindings/interrupt-controller/arm-gic.h \
  arch/arm/dts/bcm2711-rpi.dtsi \
  arch/arm/dts/bcm2835-rpi.dtsi \
  include/dt-bindings/power/raspberrypi-power.h \
  include/dt-bindings/reset/raspberrypi,firmware-reset.h \
  arch/arm/dts/nf271x-rpi-usb-peripheral.dtsi \
  arch/arm/dts/nf271x-u-boot.dtsi \

arch/arm/dts/nf2711-rpi-4-b.dtb: $(deps_arch/arm/dts/nf2711-rpi-4-b.dtb)

$(deps_arch/arm/dts/nf2711-rpi-4-b.dtb):
