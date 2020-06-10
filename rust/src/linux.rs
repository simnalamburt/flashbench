/// Please note that these FFIs works properly only in x86-64
use core::ffi::c_void;

// Types
#[repr(u32)]
#[allow(non_camel_case_types)]
pub enum GFP {
    __HIGH = 0x20,
    __ATOMIC = 0x80000,
    __KSWAPD_RECLAIM = 0x2000000,
    ATOMIC = GFP::__HIGH as u32 | GFP::__ATOMIC as u32 | GFP::__KSWAPD_RECLAIM as u32,
}

// Functions
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

    // linux/slab.h
    pub fn __kmalloc(size: usize, flags: GFP) -> *mut c_void;
    pub fn kfree(addr: *const c_void);
}
