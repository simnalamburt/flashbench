# Reference:
#   https://www.kernel.org/doc/Documentation/kbuild/modules.txt
#   https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt

ccflags-y := -I$(src)/libs
obj-m += flashBench.o

flashBench-y += \
	fb.o \
	fb_util.o \
	fb_ftl_algorithm.o \
	fb_vdevice.o \
	fb_ssd_info.o \
	fb_ftl_algorithm_page_mapping.o \
	fb_page_mapping_function.o \
	fb_gc_page_mapping.o \
	fb_write_buffer.o \
	fb_bitmap.o \
	fb_bus_controller.o

export KROOT=/lib/modules/$(shell uname -r)/build

.PHONY: default
default: modules

.PHONY: modules
modules:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules

.PHONY: modules_check
modules_check:
	@$(MAKE) -C $(KROOT) C=2 M=$(PWD) modules

.PHONY: modules_install
modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

.PHONY: kernel_clean
kernel_clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean

.PHONY: clean
clean: kernel_clean
	rm -rf Module.symvers Module.markers modules.order
