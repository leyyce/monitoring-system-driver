obj-m += monitoring_i2c.o

all: module dt
	echo Builded Device Tree Overlay and kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: monitoring_system_overlay.dts
	dtc -@ -I dts -O dtb -o monitoring_system_overlay.dtbo monitoring_system_overlay.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf monitoring_system_overlay.dtbo