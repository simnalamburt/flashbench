#![no_std]
#![feature(core_intrinsics)]

extern {
}

// TODO: Let's implement something!

/// Abort on panic
///
/// Implemented just like the [`BUG()`] macro.
///
/// [`BUG()`]: https://github.com/torvalds/linux/blob/69973b830859bc6529a7a0468ba0d80ee5117826/arch/x86/include/asm/bug.h#L30
#[panic_handler]
#[no_mangle]
extern fn panic(_info: &core::panic::PanicInfo) -> ! {
    unsafe {
        core::intrinsics::abort()
    }
}
