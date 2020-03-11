# Reference:
#   https://www.kernel.org/doc/Documentation/kbuild/modules.txt
#   https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt

obj-m += flashbench.o
flashbench-objs += \
	fb.o \
	util.o \
	ftl_algorithm.o \
	vdevice.o \
	ssd_info.o \
	ftl_algorithm_page_mapping.o \
	page_mapping_function.o \
	gc_page_mapping.o \
	write_buffer.o \
	bitmap.o \
	bus_controller.o

ccflags-y := -std=gnu11 -Wextra -Wpedantic

# To disable warnings in header files
LINUXINCLUDE := $(subst -I, -isystem, $(LINUXINCLUDE))

#
# Basic rules
#
all:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
