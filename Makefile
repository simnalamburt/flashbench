# Reference:
#   https://www.kernel.org/doc/Documentation/kbuild/modules.txt
#   https://www.kernel.org/doc/Documentation/kbuild/makefiles.txt

obj-m := flashbench.o
flashbench-objs := \
	main.o \
	flashbench.rust.o \
	util.o \
	ssd_info.o \
	ftl_algorithm_page_mapping.o \
	page_mapping_function.o \
	gc_page_mapping.o \
	write_buffer.o \
	bus_controller.o

ccflags-y := -std=gnu11 -Wall -Wextra -Wpedantic

# To disable warnings in header files
LINUXINCLUDE := $(subst -I, -isystem, $(LINUXINCLUDE))

#
# Basic rules
#
all:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

#
# Rules for Rust codes
#
$(src)/rust/target/x86_64-linux-kernel/release/libflashbench.a: $(src)/rust/Cargo.toml $(wildcard $(src)/rust/src/*.rs)
	cd $(src)/rust &&\
	  env -u MAKE -u MAKEFLAGS cargo +nightly build --release

%.rust.o: rust/target/x86_64-linux-kernel/release/lib%.a
	$(LD) -r -o $@ --whole-archive $<
