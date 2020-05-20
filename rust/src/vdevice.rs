use crate::constants::*;
use crate::linux::*;
use crate::structs::*;
use core::ffi::c_void;
use core::mem::size_of;

extern "C" {
    fn fb_bus_controller_init(ptr_vdevice: *mut vdevice_t, num_max_entries_per_chip: u32) -> i32;
    fn fb_bus_controller_destroy(ptr_bus_controller: *mut *mut fb_bus_controller_t);
    fn fb_issue_operation(
        ptr_bus_controller: *mut fb_bus_controller_t,
        chip: u32,
        operation: u32,
        ptr_bio: *mut fb_bio_t,
    ) -> i32;
}
#[no_mangle]
pub unsafe extern "C" fn create_vdevice() -> *mut vdevice_t {
    let current_block: u64;
    let mut ptr_vdevice: *mut vdevice_t;
    let mut bus_loop: u64;
    let mut chip_loop: u64;
    let mut block_loop: u64;
    let mut page_loop: u64;
    ptr_vdevice = vmalloc(size_of::<vdevice_t>()) as *mut vdevice_t;
    if ptr_vdevice.is_null() {
        printk(
            b"\x013flashbench: Allocating virtual device structure failed.\n\x00" as *const u8
                as *const i8,
        );
    } else {
        let capaticy_per_bus: usize =
            NUM_CHIPS_PER_BUS * NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK * PHYSICAL_PAGE_SIZE;
        (*ptr_vdevice).device_capacity = (NUM_BUSES * capaticy_per_bus) as u64;
        bus_loop = 0 as u64;
        loop {
            if !(bus_loop < NUM_BUSES as u64) {
                current_block = 10599921512955367680;
                break;
            }
            (*ptr_vdevice).ptr_vdisk[bus_loop as usize] =
                vmalloc(size_of::<u8>() * capaticy_per_bus) as *mut u8;
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
                let bus_capacity: usize =
                    NUM_CHIPS_PER_BUS * NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK * CFACTOR_PERCENT
                        / 100
                        * PHYSICAL_PAGE_SIZE;
                (*ptr_vdevice).logical_capacity = (NUM_BUSES * bus_capacity) as u64;
                bus_loop = 0 as u64;
                while bus_loop < NUM_BUSES as u64 {
                    chip_loop = 0 as u64;
                    while chip_loop < NUM_CHIPS_PER_BUS as u64 {
                        block_loop = 0 as u64;
                        while block_loop < NUM_BLOCKS_PER_CHIP as u64 {
                            page_loop = 0 as u64;
                            while page_loop < NUM_PAGES_PER_BLOCK as u64 {
                                (*ptr_vdevice).buses[bus_loop as usize].chips[chip_loop as usize]
                                    .blocks[block_loop as usize]
                                    .pages[page_loop as usize]
                                    .ptr_data = (*ptr_vdevice).ptr_vdisk[bus_loop as usize].offset(
                                    chip_loop
                                        .wrapping_mul(NUM_BLOCKS_PER_CHIP as u64)
                                        .wrapping_mul(NUM_PAGES_PER_BLOCK as u64)
                                        .wrapping_add(
                                            block_loop.wrapping_mul(NUM_PAGES_PER_BLOCK as u64),
                                        )
                                        .wrapping_add(page_loop)
                                        .wrapping_mul(PHYSICAL_PAGE_SIZE as u64)
                                        as isize,
                                );
                                page_loop = page_loop.wrapping_add(1)
                            }
                            block_loop = block_loop.wrapping_add(1)
                        }
                        chip_loop = chip_loop.wrapping_add(1)
                    }
                    bus_loop = bus_loop.wrapping_add(1)
                }
                if !(fb_bus_controller_init(ptr_vdevice, NUM_MAX_ENTRIES_OPR_QUEUE as u32)
                    == -(1 as i32))
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
        loop_bus = 0 as u32;
        while loop_bus < NUM_BUSES as u32 {
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
    let mut ptr_curr = ptr_dest;
    lp_loop = 0 as u8;
    while (lp_loop as i32) < NR_LP_IN_PP as i32 {
        if *page_bitmap.offset(lp_loop as isize) as i32 == 1 as i32 {
            memcpy(
                ptr_curr as *mut c_void,
                ptr_src as *const c_void,
                LOGICAL_PAGE_SIZE,
            );
            ptr_curr = ptr_curr.offset(LOGICAL_PAGE_SIZE as isize)
        }
        ptr_src = ptr_src.offset(LOGICAL_PAGE_SIZE as isize);
        lp_loop = lp_loop.wrapping_add(1)
    }
    fb_issue_operation(
        *(*ptr_vdevice).ptr_bus_controller.offset(bus as isize),
        chip as u32,
        OP_READ as u32,
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
        PHYSICAL_PAGE_SIZE,
    );
    fb_issue_operation(
        *(*ptr_vdevice).ptr_bus_controller.offset(bus as isize),
        chip as u32,
        OP_PROG as u32,
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
        .pages[0 as usize]
        .ptr_data;
    memset(
        ptr_dest as *mut c_void,
        0xff as i32,
        NUM_PAGES_PER_BLOCK * PHYSICAL_PAGE_SIZE,
    );
    fb_issue_operation(
        *(*ptr_vdevice).ptr_bus_controller.offset(bus as isize),
        chip as u32,
        OP_BERS as u32,
        ptr_fb_bio,
    );
}
#[no_mangle]
pub unsafe extern "C" fn is_valid_address_range(logical_page_address: u32) -> i32 {
    return (logical_page_address < NUM_LOG_PAGES as u32) as i32;
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
    return bus << 28 as i32 | chip << 24 as i32 | block << 12 as i32 | page;
}
#[no_mangle]
pub unsafe extern "C" fn convert_to_ssd_layout(
    logical_page_address: u32,
    ptr_bus: *mut u32,
    ptr_chip: *mut u32,
    ptr_block: *mut u32,
    ptr_page: *mut u32,
) {
    *ptr_bus = 0xf as u32 & logical_page_address >> 28 as i32;
    *ptr_chip = 0xf as u32 & logical_page_address >> 24 as i32;
    *ptr_block = 0xfff as u32 & logical_page_address >> 12 as i32;
    *ptr_page = 0xfff as u32 & logical_page_address;
}
#[no_mangle]
pub unsafe extern "C" fn operation_time(op: fb_dev_op_t) -> u32 {
    let tread = 80u32;
    let tprog = 800u32;
    let tbers = 5000u32;
    let tplock = 100u32;
    let tblock = 100u32;

    match op as u32 {
        0 => return tread,
        1 => return tprog,
        2 => return tplock,
        3 => return tblock,
        4 => return tbers,
        _ => return 0u32,
    };
}
