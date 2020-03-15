#![no_std]
#![register_tool(c2rust)]
#![feature(core_intrinsics, extern_types, register_tool)]

mod gc_page_mapping;
mod page_mapping_function;
mod vdevice;

pub use gc_page_mapping::*;
pub use page_mapping_function::*;
pub use vdevice::*;

/// Abort on panic
///
/// Implemented just like the [`BUG()`] macro.
///
/// [`BUG()`]: https://github.com/torvalds/linux/blob/69973b830859bc6529a7a0468ba0d80ee5117826/arch/x86/include/asm/bug.h#L30
#[panic_handler]
#[no_mangle]
extern "C" fn panic(_info: &core::panic::PanicInfo) -> ! {
    unsafe { core::intrinsics::abort() }
}
