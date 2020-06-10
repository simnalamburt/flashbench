//! Slab Allocator wrapper is defined in this module. See references for the further details.
//!
//! ##### References
//! - https://www.kernel.org/doc/gorman/html/understand/understand011.html
//! - https://www.kernel.org/doc/htmldocs/kernel-api/API-kmalloc.html

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;
use core::mem;

use crate::linux;

/// Assumes a condition that always must hold. Hint's the compiler optimizer.
macro_rules! assume {
    ($e:expr) => {
        debug_assert!($e);
        if !($e) {
            core::hint::unreachable_unchecked();
        }
    };
}

// NOTE: This is true only in x86-64
const KMALLOC_ALIGN: usize = mem::align_of::<u64>();

struct SlabAllocator;

#[global_allocator]
static SLAB: SlabAllocator = SlabAllocator;

unsafe impl GlobalAlloc for SlabAllocator {
    // TODO: `__attribute__((malloc))` exists in real kmalloc()
    //
    // Reference:
    //   https://gcc.gnu.org/onlinedocs/gcc-10.1.0/gcc/Common-Function-Attributes.html#index-functions-that-behave-like-malloc
    #[inline]
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        assume!(layout.size() != 0);

        let size = layout.size();
        let align = layout.align();

        if align <= KMALLOC_ALIGN {
            let ptr = linux::__kmalloc(size, linux::GFP::ATOMIC);
            assume!(ptr as usize % KMALLOC_ALIGN == 0);
            ptr as *mut u8
        } else {
            // Padding should be added to align the memory block properly.
            // And extra space is required to store the location of original pointer.
            let pad = layout.align() - KMALLOC_ALIGN + mem::size_of::<usize>();
            let addr = linux::__kmalloc(size + pad, linux::GFP::ATOMIC) as usize;

            // Align the memory as required
            let aligned = (addr + align - 1) & !(align - 1);
            assume!(aligned % align == 0);

            // Store the original pointer
            *((aligned + size) as *mut usize) = addr;

            aligned as *mut u8
        }
    }

    #[inline]
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        assume!(!ptr.is_null());
        assume!(layout.size() != 0);

        let align = layout.align();
        if align <= KMALLOC_ALIGN {
            linux::kfree(ptr as *const c_void);
        } else {
            // Extract the original address
            let addr = *((ptr as usize + layout.size()) as *const usize);
            linux::kfree(addr as *const c_void);
        }
    }
}
