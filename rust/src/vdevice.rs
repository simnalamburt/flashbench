use core::ffi::c_void;
use crate::constants::*;

extern "C" {
    pub type fb_bus_controller_t;
    pub type fb_bio_t;
    #[no_mangle]
    fn printk(fmt: *const i8, _: ...) -> i32;
    #[no_mangle]
    fn memset(_: *mut c_void, _: i32, _: u64) -> *mut c_void;
    #[no_mangle]
    fn memcpy(_: *mut c_void, _: *const c_void, _: u64) -> *mut c_void;
    #[no_mangle]
    fn vmalloc(size: u64) -> *mut c_void;
    #[no_mangle]
    fn vfree(addr: *const c_void);
    #[no_mangle]
    fn fb_bus_controller_init(
        ptr_vdevice: *mut vdevice_t,
        num_max_entries_per_chip: u32,
    ) -> i32;
    #[no_mangle]
    fn fb_bus_controller_destroy(ptr_bus_controller: *mut *mut fb_bus_controller_t);
    #[no_mangle]
    fn fb_issue_operation(
        ptr_bus_controller: *mut fb_bus_controller_t,
        chip: u32,
        operation: u32,
        ptr_bio: *mut fb_bio_t,
    ) -> i32;
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct vdevice_t {
    pub device_capacity: u64,
    pub logical_capacity: u64,
    pub ptr_vdisk: [*mut u8; 1],
    pub buses: [vdevice_bus_t; 1],
    pub ptr_bus_controller: *mut *mut fb_bus_controller_t,
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct vdevice_bus_t {
    pub chips: [vdevice_chip_t; 2],
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct vdevice_chip_t {
    pub blocks: [vdevice_block_t; 171],
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct vdevice_block_t {
    pub pages: [vdevice_page_t; 192],
}
#[derive(Copy, Clone)]
#[repr(C)]
pub struct vdevice_page_t {
    pub ptr_data: *mut u8,
}
#[no_mangle]
pub unsafe extern "C" fn create_vdevice() -> *mut vdevice_t {
    let current_block: u64;
    let mut ptr_vdevice: *mut vdevice_t;
    let mut bus_loop: u64;
    let mut chip_loop: u64;
    let mut block_loop: u64;
    let mut page_loop: u64;
    let mut bus_capacity: u64;
    ptr_vdevice = vmalloc(::core::mem::size_of::<vdevice_t>() as u64) as *mut vdevice_t;
    if ptr_vdevice.is_null() {
        printk(
            b"\x013flashbench: Allocating virtual device structure failed.\n\x00" as *const u8
                as *const i8,
        );
    } else {
        bus_capacity = NUM_CHIPS_PER_BUS as i32 as u64;
        bus_capacity = (bus_capacity as u64)
            .wrapping_mul(NUM_BLOCKS_PER_CHIP as i32 as u64)
            as u64 as u64;
        bus_capacity = (bus_capacity as u64)
            .wrapping_mul(NUM_PAGES_PER_BLOCK as i32 as u64)
            as u64 as u64;
        bus_capacity = (bus_capacity as u64)
            .wrapping_mul(PHYSICAL_PAGE_SIZE as i32 as u64)
            as u64 as u64;
        (*ptr_vdevice).device_capacity =
            (NUM_BUSES as i32 as u64).wrapping_mul(bus_capacity);
        bus_loop = 0 as i32 as u64;
        loop {
            if !(bus_loop < NUM_BUSES as i32 as u64) {
                current_block = 10599921512955367680;
                break;
            }
            (*ptr_vdevice).ptr_vdisk[bus_loop as usize] = vmalloc(
                (::core::mem::size_of::<u8>() as u64 as u64)
                    .wrapping_mul(bus_capacity) as u64,
            ) as *mut u8;
            if (*ptr_vdevice).ptr_vdisk[bus_loop as usize].is_null() {
                printk(
                    b"\x013flashbench: Allocating virtual disk failed.\n\x00" as *const u8
                        as *const i8,
                );
                current_block = 7640765176936679807;
                break;
            } else {
                bus_loop = bus_loop.wrapping_add(1)
            }
        }
        match current_block {
            10599921512955367680 => {
                bus_capacity = NUM_CHIPS_PER_BUS as i32 as u64;
                bus_capacity = (bus_capacity as u64)
                    .wrapping_mul(NUM_BLOCKS_PER_CHIP as i32 as u64)
                    as u64 as u64;
                bus_capacity = (bus_capacity as u64)
                    .wrapping_mul(NUM_PAGES_PER_BLOCK as i32 as u64)
                    as u64 as u64;
                bus_capacity = bus_capacity
                    .wrapping_mul(CFACTOR_PERCENT as i32 as u64)
                    .wrapping_div(100 as i32 as u64);
                bus_capacity = (bus_capacity as u64)
                    .wrapping_mul(PHYSICAL_PAGE_SIZE as i32 as u64)
                    as u64 as u64;
                (*ptr_vdevice).logical_capacity =
                    (NUM_BUSES as i32 as u64).wrapping_mul(bus_capacity);
                bus_loop = 0 as i32 as u64;
                while bus_loop < NUM_BUSES as i32 as u64 {
                    chip_loop = 0 as i32 as u64;
                    while chip_loop < NUM_CHIPS_PER_BUS as i32 as u64 {
                        block_loop = 0 as i32 as u64;
                        while block_loop < NUM_BLOCKS_PER_CHIP as i32 as u64 {
                            page_loop = 0 as i32 as u64;
                            while page_loop
                                < NUM_PAGES_PER_BLOCK as i32 as u64
                            {
                                (*ptr_vdevice).buses[bus_loop as usize].chips[chip_loop as usize]
                                    .blocks[block_loop as usize]
                                    .pages[page_loop as usize]
                                    .ptr_data = (*ptr_vdevice).ptr_vdisk[bus_loop as usize].offset(
                                    chip_loop
                                        .wrapping_mul(
                                            NUM_BLOCKS_PER_CHIP as i32 as u64,
                                        )
                                        .wrapping_mul(
                                            NUM_PAGES_PER_BLOCK as i32 as u64,
                                        )
                                        .wrapping_add(block_loop.wrapping_mul(
                                            NUM_PAGES_PER_BLOCK as i32 as u64,
                                        ))
                                        .wrapping_add(page_loop)
                                        .wrapping_mul(
                                            PHYSICAL_PAGE_SIZE as i32 as u64,
                                        ) as isize,
                                );
                                page_loop = page_loop.wrapping_add(1)
                            }
                            block_loop = block_loop.wrapping_add(1)
                        }
                        chip_loop = chip_loop.wrapping_add(1)
                    }
                    bus_loop = bus_loop.wrapping_add(1)
                }
                if !(fb_bus_controller_init(
                    ptr_vdevice,
                    NUM_MAX_ENTRIES_OPR_QUEUE as i32 as u32,
                ) == -(1 as i32))
                {
                    return ptr_vdevice;
                }
            }
            _ => {}
        }
        if !ptr_vdevice.is_null() {
            vfree(ptr_vdevice as *const c_void);
        }
    }
    return 0 as *mut vdevice_t;
}
#[no_mangle]
pub unsafe extern "C" fn destroy_vdevice(ptr_vdevice: *mut vdevice_t) {
    let mut loop_bus: u32;
    if !ptr_vdevice.is_null() {
        fb_bus_controller_destroy((*ptr_vdevice).ptr_bus_controller);
        loop_bus = 0 as i32 as u32;
        while loop_bus < NUM_BUSES as i32 as u32 {
            if !(*ptr_vdevice).ptr_vdisk[loop_bus as usize].is_null() {
                vfree((*ptr_vdevice).ptr_vdisk[loop_bus as usize] as *const c_void);
            }
            loop_bus = loop_bus.wrapping_add(1)
        }
        vfree(ptr_vdevice as *const c_void);
    };
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_read(
    ptr_vdevice: *mut vdevice_t,
    bus: u8,
    chip: u8,
    block: u32,
    page: u32,
    page_bitmap: *mut u8,
    ptr_dest: *mut u8,
    ptr_fb_bio: *mut fb_bio_t,
) {
    let mut ptr_src: *mut u8 = (*ptr_vdevice).buses[bus as usize].chips[chip as usize].blocks
        [block as usize]
        .pages[page as usize]
        .ptr_data;
    let mut lp_loop: u8;
    let mut ptr_curr: *mut u8 = ptr_dest;
    lp_loop = 0 as i32 as u8;
    while (lp_loop as i32) < NR_LP_IN_PP as i32 {
        if *page_bitmap.offset(lp_loop as isize) as i32 == 1 as i32 {
            memcpy(
                ptr_curr as *mut c_void,
                ptr_src as *const c_void,
                LOGICAL_PAGE_SIZE as i32 as u64,
            );
            ptr_curr = ptr_curr.offset(LOGICAL_PAGE_SIZE as i32 as isize)
        }
        ptr_src = ptr_src.offset(LOGICAL_PAGE_SIZE as i32 as isize);
        lp_loop = lp_loop.wrapping_add(1)
    }
    fb_issue_operation(
        *(*ptr_vdevice).ptr_bus_controller.offset(bus as isize),
        chip as u32,
        OP_READ as i32 as u32,
        ptr_fb_bio,
    );
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_write(
    ptr_vdevice: *mut vdevice_t,
    bus: u8,
    chip: u8,
    block: u32,
    page: u32,
    ptr_src: *const u8,
    ptr_fb_bio: *mut fb_bio_t,
) {
    let ptr_dest: *mut u8 = (*ptr_vdevice).buses[bus as usize].chips[chip as usize].blocks
        [block as usize]
        .pages[page as usize]
        .ptr_data;
    memcpy(
        ptr_dest as *mut c_void,
        ptr_src as *const c_void,
        PHYSICAL_PAGE_SIZE as i32 as u64,
    );
    fb_issue_operation(
        *(*ptr_vdevice).ptr_bus_controller.offset(bus as isize),
        chip as u32,
        OP_PROG as i32 as u32,
        ptr_fb_bio,
    );
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_erase(
    ptr_vdevice: *mut vdevice_t,
    bus: u8,
    chip: u8,
    block: u32,
    ptr_fb_bio: *mut fb_bio_t,
) {
    let ptr_dest: *mut u8 = (*ptr_vdevice).buses[bus as usize].chips[chip as usize].blocks
        [block as usize]
        .pages[0 as i32 as usize]
        .ptr_data;
    memset(
        ptr_dest as *mut c_void,
        0xff as i32,
        (NUM_PAGES_PER_BLOCK as i32 * PHYSICAL_PAGE_SIZE as i32) as u64,
    );
    fb_issue_operation(
        *(*ptr_vdevice).ptr_bus_controller.offset(bus as isize),
        chip as u32,
        OP_BERS as i32 as u32,
        ptr_fb_bio,
    );
}
#[no_mangle]
pub unsafe extern "C" fn is_valid_address_range(logical_page_address: u32) -> i32 {
    return (logical_page_address < NUM_LOG_PAGES as i32 as u32) as i32;
}
#[no_mangle]
pub unsafe extern "C" fn convert_to_physical_address(
    bus: u32,
    chip: u32,
    block: u32,
    page: u32,
) -> u32 {
    // return (bus << 27) | (chip << 21) | (block << 8) | page;
    // Max buses: 16 - 4
    // Max chips: 16 - 4
    // Max blocks: 4096 - 12
    // Max pages: 4096 - 12
    return bus << 28 as i32
        | chip << 24 as i32
        | block << 12 as i32
        | page;
}
#[no_mangle]
pub unsafe extern "C" fn convert_to_ssd_layout(
    logical_page_address: u32,
    ptr_bus: *mut u32,
    ptr_chip: *mut u32,
    ptr_block: *mut u32,
    ptr_page: *mut u32,
) {
    *ptr_bus = 0xf as i32 as u32 & logical_page_address >> 28 as i32;
    *ptr_chip = 0xf as i32 as u32 & logical_page_address >> 24 as i32;
    *ptr_block = 0xfff as i32 as u32 & logical_page_address >> 12 as i32;
    *ptr_page = 0xfff as i32 as u32 & logical_page_address;
}
#[no_mangle]
pub unsafe extern "C" fn operation_time(op: fb_dev_op_t) -> u32 {
    match op as u32 {
        0 => return TREAD as i32 as u32,
        1 => return TPROG as i32 as u32,
        2 => return TPLOCK as i32 as u32,
        3 => return TBLOCK as i32 as u32,
        4 => return TBERS as i32 as u32,
        _ => return 0 as i32 as u32,
    };
}