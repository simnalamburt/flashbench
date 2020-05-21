use crate::constants::*;
use crate::linux::*;
use crate::structs::*;
use core::ffi::c_void;
use core::mem::size_of;
use core::ptr;

extern "C" {
    fn fb_bus_controller_init(ptr_vdevice: *mut vdevice_t, num_max_entries_per_chip: u32) -> i32;
    fn fb_bus_controller_destroy(ptr_bus_controller: *mut *mut fb_bus_controller_t);
    fn fb_issue_operation(
        ptr_bus_controller: *mut fb_bus_controller_t,
        chip: u32,
        operation: fb_dev_op_t,
        ptr_bio: *mut fb_bio_t,
    ) -> i32;
}

#[no_mangle]
pub unsafe extern "C" fn create_vdevice() -> *mut vdevice_t {
    let ptr_vdevice = vmalloc(size_of::<vdevice_t>()) as *mut vdevice_t;
    if ptr_vdevice.is_null() {
        printk(
            b"\x013flashbench: Allocating virtual device structure failed.\n\x00" as *const u8
                as *const i8,
        );
        return ptr::null_mut();
    }

    const CAPATICY_PER_BUS: usize =
        NUM_CHIPS_PER_BUS * NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK * PHYSICAL_PAGE_SIZE;
    (*ptr_vdevice).device_capacity = (NUM_BUSES * CAPATICY_PER_BUS) as u64;

    let mut bus_loop = 0;
    while bus_loop < NUM_BUSES {
        let ptr = vmalloc(CAPATICY_PER_BUS) as *mut u8;
        if ptr.is_null() {
            printk(
                b"\x013flashbench: Allocating virtual disk failed.\n\x00" as *const u8 as *const i8,
            );
            // TODO: ptr_vdevice만 해제하는게 아니라, ptr_vdisk[] 안에있는것도 모조리 해제해야함
            if !ptr_vdevice.is_null() {
                vfree(ptr_vdevice as *const c_void);
            }
            return ptr::null_mut();
        }

        (*ptr_vdevice).ptr_vdisk[bus_loop] = ptr;
        bus_loop += 1;
    }

    (*ptr_vdevice).logical_capacity =
        (NUM_PAGES * CFACTOR_PERCENT / 100 * PHYSICAL_PAGE_SIZE) as u64;
    let mut bus_loop = 0;
    while bus_loop < NUM_BUSES {
        let mut chip_loop = 0;
        while chip_loop < NUM_CHIPS_PER_BUS {
            let mut block_loop = 0;
            while block_loop < NUM_BLOCKS_PER_CHIP {
                let mut page_loop = 0;
                while page_loop < NUM_PAGES_PER_BLOCK {
                    (*ptr_vdevice).buses[bus_loop].chips[chip_loop].blocks[block_loop].pages
                        [page_loop]
                        .ptr_data = (*ptr_vdevice).ptr_vdisk[bus_loop].offset(
                        ((chip_loop * NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK
                            + block_loop * NUM_PAGES_PER_BLOCK
                            + page_loop)
                            * PHYSICAL_PAGE_SIZE) as isize,
                    );
                    page_loop += 1;
                }
                block_loop += 1;
            }
            chip_loop += 1;
        }
        bus_loop += 1;
    }

    if fb_bus_controller_init(ptr_vdevice, NUM_MAX_ENTRIES_OPR_QUEUE as u32) == -1 {
        // TODO: ptr_vdevice만 해제하는게 아니라, ptr_vdisk[] 안에있는것도 모조리 해제해야함
        if !ptr_vdevice.is_null() {
            vfree(ptr_vdevice as *const c_void);
        }
        return ptr::null_mut();
    }

    return ptr_vdevice;
}

#[no_mangle]
pub unsafe extern "C" fn destroy_vdevice(ptr_vdevice: *mut vdevice_t) {
    if ptr_vdevice.is_null() {
        return;
    }

    fb_bus_controller_destroy((*ptr_vdevice).ptr_bus_controller);
    let mut loop_bus = 0;
    while loop_bus < NUM_BUSES {
        let ptr = (*ptr_vdevice).ptr_vdisk[loop_bus];
        if !ptr.is_null() {
            vfree(ptr as *const c_void);
        }
        loop_bus += 1;
    }
    vfree(ptr_vdevice as *const c_void);
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
        OP_READ,
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
        OP_PROG,
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
        OP_BERS,
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
    // Max buses: 16 - 4
    // Max chips: 16 - 4
    // Max blocks: 4096 - 12
    // Max pages: 4096 - 12
    return bus << 28 | chip << 24 | block << 12 | page;
}

#[no_mangle]
pub unsafe extern "C" fn convert_to_ssd_layout(
    logical_page_address: u32,
    ptr_bus: *mut u32,
    ptr_chip: *mut u32,
    ptr_block: *mut u32,
    ptr_page: *mut u32,
) {
    *ptr_bus = 0xf & logical_page_address >> 28;
    *ptr_chip = 0xf & logical_page_address >> 24;
    *ptr_block = 0xfff & logical_page_address >> 12;
    *ptr_page = 0xfff & logical_page_address;
}

#[no_mangle]
pub unsafe extern "C" fn operation_time(op: fb_dev_op_t) -> u32 {
    match op {
        fb_dev_op_t::OP_READ => 80,
        fb_dev_op_t::OP_PROG => 800,
        fb_dev_op_t::OP_PLOCK => 5000,
        fb_dev_op_t::OP_BLOCK => 100,
        fb_dev_op_t::OP_BERS => 100,
    }
}
