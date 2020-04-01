#![allow(dead_code, mutable_transmutes, non_camel_case_types, non_snake_case,
         non_upper_case_globals, unused_assignments, unused_mut)]

extern "C" {
    pub type fb_bus_controller_t;
    pub type fb_bio_t;
    #[no_mangle]
    fn printk(fmt: *const i8, _: ...) -> i32;
    #[no_mangle]
    fn memset(_: *mut core::ffi::c_void, _: i32, _: u64) -> *mut core::ffi::c_void;
    #[no_mangle]
    fn memcpy(_: *mut core::ffi::c_void, _: *const core::ffi::c_void, _: u64) -> *mut core::ffi::c_void;
    #[no_mangle]
    fn vmalloc(size: u64) -> *mut core::ffi::c_void;
    #[no_mangle]
    fn vfree(addr: *const core::ffi::c_void);
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
pub type C2RustUnnamed = u32;
pub const BGC_TH_NR_BLKS: C2RustUnnamed = 14;
pub const BGC_TH_WB_UTIL: C2RustUnnamed = 5;
pub const BGC_TH_INTV: C2RustUnnamed = 5000;
pub const NUM_MAX_ENTRIES_OPR_QUEUE: C2RustUnnamed = 4;
pub const TBLOCK: C2RustUnnamed = 100;
pub const TPLOCK: C2RustUnnamed = 100;
pub const TBERS: C2RustUnnamed = 5000;
pub const TPROG: C2RustUnnamed = 800;
pub const TREAD: C2RustUnnamed = 80;
pub const SECTOR_SIZE: C2RustUnnamed = 512;
pub const PHYSICAL_PAGE_SIZE: C2RustUnnamed = 16384;
pub const LOGICAL_PAGE_SIZE: C2RustUnnamed = 4096;
pub const NUM_LOG_PAGES: C2RustUnnamed = 236390;
pub const NUM_PAGES_IN_WRITE_BUFFER: C2RustUnnamed = 8;
pub const NUM_PAGES: C2RustUnnamed = 65664;
pub const NUM_BLOCKS: C2RustUnnamed = 342;
pub const NUM_CHIPS: C2RustUnnamed = 2;
pub const LP_PAGE_SHIFT: C2RustUnnamed = 2;
pub const LP_PAGE_MASK: C2RustUnnamed = 3;
pub const NR_LP_IN_PP: C2RustUnnamed = 4;
// v-layer * h-layer * multi-level degree
pub const CFACTOR_PERCENT: C2RustUnnamed = 90;
pub const NUM_PAGES_PER_BLOCK: C2RustUnnamed = 192;
pub const NUM_BLOCKS_PER_CHIP: C2RustUnnamed = 171;
pub const NUM_CHIPS_PER_BUS: C2RustUnnamed = 2;
//
// Hardware configuration
//
pub const NUM_BUSES: C2RustUnnamed = 1;
pub const NR_MAX_LPGS_COPY: C2RustUnnamed = 6144;
pub const NUM_BTODS: C2RustUnnamed = 2048;
pub const NUM_WTODS: C2RustUnnamed = 2048;
//
// Parameterf for DEL manager
//
pub const NR_MAX_LPAS_DISCARD: C2RustUnnamed = 2048;
pub type fb_dev_op_t = u32;
pub const OP_BERS: fb_dev_op_t = 4;
pub const OP_BLOCK: fb_dev_op_t = 3;
pub const OP_PLOCK: fb_dev_op_t = 2;
pub const OP_PROG: fb_dev_op_t = 1;
pub const OP_READ: fb_dev_op_t = 0;
#[no_mangle]
pub unsafe extern "C" fn create_vdevice() -> *mut vdevice_t {
    let mut current_block: u64;
    let mut ptr_vdevice: *mut vdevice_t = 0 as *mut vdevice_t;
    let mut bus_loop: u64 = 0;
    let mut chip_loop: u64 = 0;
    let mut block_loop: u64 = 0;
    let mut page_loop: u64 = 0;
    let mut bus_capacity: u64 = 0;
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
            vfree(ptr_vdevice as *const core::ffi::c_void);
        }
    }
    return 0 as *mut vdevice_t;
}
#[no_mangle]
pub unsafe extern "C" fn destroy_vdevice(mut ptr_vdevice: *mut vdevice_t) {
    let mut loop_bus: u32 = 0;
    if !ptr_vdevice.is_null() {
        fb_bus_controller_destroy((*ptr_vdevice).ptr_bus_controller);
        loop_bus = 0 as i32 as u32;
        while loop_bus < NUM_BUSES as i32 as u32 {
            if !(*ptr_vdevice).ptr_vdisk[loop_bus as usize].is_null() {
                vfree((*ptr_vdevice).ptr_vdisk[loop_bus as usize] as *const core::ffi::c_void);
            }
            loop_bus = loop_bus.wrapping_add(1)
        }
        vfree(ptr_vdevice as *const core::ffi::c_void);
    };
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_read(
    mut ptr_vdevice: *mut vdevice_t,
    mut bus: u8,
    mut chip: u8,
    mut block: u32,
    mut page: u32,
    mut page_bitmap: *mut u8,
    mut ptr_dest: *mut u8,
    mut ptr_fb_bio: *mut fb_bio_t,
) {
    let mut ptr_src: *mut u8 = (*ptr_vdevice).buses[bus as usize].chips[chip as usize].blocks
        [block as usize]
        .pages[page as usize]
        .ptr_data;
    let mut lp_loop: u8 = 0;
    let mut ptr_curr: *mut u8 = ptr_dest;
    lp_loop = 0 as i32 as u8;
    while (lp_loop as i32) < NR_LP_IN_PP as i32 {
        if *page_bitmap.offset(lp_loop as isize) as i32 == 1 as i32 {
            memcpy(
                ptr_curr as *mut core::ffi::c_void,
                ptr_src as *const core::ffi::c_void,
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
    mut ptr_vdevice: *mut vdevice_t,
    mut bus: u8,
    mut chip: u8,
    mut block: u32,
    mut page: u32,
    mut ptr_src: *const u8,
    mut ptr_fb_bio: *mut fb_bio_t,
) {
    let mut ptr_dest: *mut u8 = (*ptr_vdevice).buses[bus as usize].chips[chip as usize].blocks
        [block as usize]
        .pages[page as usize]
        .ptr_data;
    memcpy(
        ptr_dest as *mut core::ffi::c_void,
        ptr_src as *const core::ffi::c_void,
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
    mut ptr_vdevice: *mut vdevice_t,
    mut bus: u8,
    mut chip: u8,
    mut block: u32,
    mut ptr_fb_bio: *mut fb_bio_t,
) {
    let mut ptr_dest: *mut u8 = (*ptr_vdevice).buses[bus as usize].chips[chip as usize].blocks
        [block as usize]
        .pages[0 as i32 as usize]
        .ptr_data;
    memset(
        ptr_dest as *mut core::ffi::c_void,
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
pub unsafe extern "C" fn is_valid_address_range(mut logical_page_address: u32) -> i32 {
    return (logical_page_address < NUM_LOG_PAGES as i32 as u32) as i32;
}
#[no_mangle]
pub unsafe extern "C" fn convert_to_physical_address(
    mut bus: u32,
    mut chip: u32,
    mut block: u32,
    mut page: u32,
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
    mut logical_page_address: u32,
    mut ptr_bus: *mut u32,
    mut ptr_chip: *mut u32,
    mut ptr_block: *mut u32,
    mut ptr_page: *mut u32,
) {
    *ptr_bus = 0xf as i32 as u32 & logical_page_address >> 28 as i32;
    *ptr_chip = 0xf as i32 as u32 & logical_page_address >> 24 as i32;
    *ptr_block = 0xfff as i32 as u32 & logical_page_address >> 12 as i32;
    *ptr_page = 0xfff as i32 as u32 & logical_page_address;
}
#[no_mangle]
pub unsafe extern "C" fn operation_time(mut op: fb_dev_op_t) -> u32 {
    match op as u32 {
        0 => return TREAD as i32 as u32,
        1 => return TPROG as i32 as u32,
        2 => return TPLOCK as i32 as u32,
        3 => return TBLOCK as i32 as u32,
        4 => return TBERS as i32 as u32,
        _ => return 0 as i32 as u32,
    };
}
