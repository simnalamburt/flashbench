use core::ffi::c_void;

extern "C" {
    // linux/printk.h
    // TODO: Change into *const u8
    pub fn printk(fmt: *const i8, _: ...) -> i32;

    // linux/vmalloc.h
    pub fn vmalloc(size: usize) -> *mut c_void;
    pub fn vfree(addr: *const c_void);

    pub fn memset(_: *mut c_void, _: i32, _: usize) -> *mut c_void;
    pub fn memcpy(_: *mut c_void, _: *const c_void, _: usize) -> *mut c_void;
}
