use core::ffi::c_void;

extern "C" {
    // linux/printk.h
    pub fn printk(fmt: *const i8, _: ...) -> i32;

    // linux/vmalloc.h
    pub fn vmalloc(size: u64) -> *mut c_void;
    pub fn vfree(addr: *const c_void);

    pub fn memset(_: *mut c_void, _: i32, _: u64) -> *mut c_void;
    pub fn memcpy(_: *mut c_void, _: *const c_void, _: u64) -> *mut c_void;
}
