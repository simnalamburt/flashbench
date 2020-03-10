# vim: ft=make
#
# Reference:
#   https://www.kernel.org/doc/Documentation/kbuild/modules.txt
#   https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt

obj-m += flashBench.o
flashBench-objs += \
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

ccflags-y := -I$(src)/libs

# To disable warnings in header files
LINUXINCLUDE := $(subst -I, -isystem, $(LINUXINCLUDE))
