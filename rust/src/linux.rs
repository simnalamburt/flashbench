use core::ffi::c_void;

extern "C" {
    // linux/printk.h
    // TODO: Change into *const u8
    pub fn printk(fmt: *const i8, arguments: ...) -> i32;

    // linux/vmalloc.h
    pub fn vmalloc(size: usize) -> *mut c_void;
    pub fn vfree(addr: *const c_void);

    // linux/string.h
    pub fn memset(addr: *mut c_void, byte: i32, size: usize) -> *mut c_void;
    pub fn memcpy(dest: *mut c_void, src: *const c_void, size: usize) -> *mut c_void;
}
