#![allow(dead_code, mutable_transmutes, non_camel_case_types, non_snake_case,
         non_upper_case_globals, unused_assignments, unused_mut)]
#![register_tool(c2rust)]
#![feature(extern_types, register_tool)]
extern "C" {
    pub type fb_bus_controller_t;
    pub type fb_bio_t;
    #[no_mangle]
    fn printk(fmt: *const libc::c_char, _: ...) -> libc::c_int;
    #[no_mangle]
    fn memset(_: *mut libc::c_void, _: libc::c_int, _: libc::c_ulong)
     -> *mut libc::c_void;
    #[no_mangle]
    fn memcpy(_: *mut libc::c_void, _: *const libc::c_void, _: libc::c_ulong)
     -> *mut libc::c_void;
    #[no_mangle]
    fn vmalloc(size: libc::c_ulong) -> *mut libc::c_void;
    #[no_mangle]
    fn vfree(addr: *const libc::c_void);
    #[no_mangle]
    fn fb_bus_controller_init(ptr_vdevice: *mut vdevice_t,
                              num_max_entries_per_chip: u32_0) -> libc::c_int;
    #[no_mangle]
    fn fb_bus_controller_destroy(ptr_bus_controller:
                                     *mut *mut fb_bus_controller_t);
    #[no_mangle]
    fn fb_issue_operation(ptr_bus_controller: *mut fb_bus_controller_t,
                          chip: u32_0, operation: u32_0,
                          ptr_bio: *mut fb_bio_t) -> libc::c_int;
}
pub type u8_0 = libc::c_uchar;
pub type u32_0 = libc::c_uint;
pub type u64_0 = libc::c_ulonglong;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct vdevice_t {
    pub device_capacity: u64_0,
    pub logical_capacity: u64_0,
    pub ptr_vdisk: [*mut u8_0; 1],
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
    pub ptr_data: *mut u8_0,
}
pub type C2RustUnnamed = libc::c_uint;
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
pub type fb_dev_op_t = libc::c_uint;
pub const OP_BERS: fb_dev_op_t = 4;
pub const OP_BLOCK: fb_dev_op_t = 3;
pub const OP_PLOCK: fb_dev_op_t = 2;
pub const OP_PROG: fb_dev_op_t = 1;
pub const OP_READ: fb_dev_op_t = 0;
#[no_mangle]
pub unsafe extern "C" fn create_vdevice() -> *mut vdevice_t {
    let mut current_block: u64;
    let mut ptr_vdevice: *mut vdevice_t = 0 as *mut vdevice_t;
    let mut bus_loop: u64_0 = 0;
    let mut chip_loop: u64_0 = 0;
    let mut block_loop: u64_0 = 0;
    let mut page_loop: u64_0 = 0;
    let mut bus_capacity: u64_0 = 0;
    ptr_vdevice =
        vmalloc(::std::mem::size_of::<vdevice_t>() as libc::c_ulong) as
            *mut vdevice_t;
    if ptr_vdevice.is_null() {
        printk(b"\x013flashbench: Allocating virtual device structure failed.\n\x00"
                   as *const u8 as *const libc::c_char);
    } else {
        bus_capacity = NUM_CHIPS_PER_BUS as libc::c_int as u64_0;
        bus_capacity =
            (bus_capacity as
                 libc::c_ulonglong).wrapping_mul(NUM_BLOCKS_PER_CHIP as
                                                     libc::c_int as
                                                     libc::c_ulonglong) as
                u64_0 as u64_0;
        bus_capacity =
            (bus_capacity as
                 libc::c_ulonglong).wrapping_mul(NUM_PAGES_PER_BLOCK as
                                                     libc::c_int as
                                                     libc::c_ulonglong) as
                u64_0 as u64_0;
        bus_capacity =
            (bus_capacity as
                 libc::c_ulonglong).wrapping_mul(PHYSICAL_PAGE_SIZE as
                                                     libc::c_int as
                                                     libc::c_ulonglong) as
                u64_0 as u64_0;
        (*ptr_vdevice).device_capacity =
            (NUM_BUSES as libc::c_int as
                 libc::c_ulonglong).wrapping_mul(bus_capacity);
        bus_loop = 0 as libc::c_int as u64_0;
        loop  {
            if !(bus_loop < NUM_BUSES as libc::c_int as libc::c_ulonglong) {
                current_block = 10599921512955367680;
                break ;
            }
            (*ptr_vdevice).ptr_vdisk[bus_loop as usize] =
                vmalloc((::std::mem::size_of::<u8_0>() as libc::c_ulong as
                             libc::c_ulonglong).wrapping_mul(bus_capacity) as
                            libc::c_ulong) as *mut u8_0;
            if (*ptr_vdevice).ptr_vdisk[bus_loop as usize].is_null() {
                printk(b"\x013flashbench: Allocating virtual disk failed.\n\x00"
                           as *const u8 as *const libc::c_char);
                current_block = 7640765176936679807;
                break ;
            } else { bus_loop = bus_loop.wrapping_add(1) }
        }
        match current_block {
            10599921512955367680 => {
                bus_capacity = NUM_CHIPS_PER_BUS as libc::c_int as u64_0;
                bus_capacity =
                    (bus_capacity as
                         libc::c_ulonglong).wrapping_mul(NUM_BLOCKS_PER_CHIP
                                                             as libc::c_int as
                                                             libc::c_ulonglong)
                        as u64_0 as u64_0;
                bus_capacity =
                    (bus_capacity as
                         libc::c_ulonglong).wrapping_mul(NUM_PAGES_PER_BLOCK
                                                             as libc::c_int as
                                                             libc::c_ulonglong)
                        as u64_0 as u64_0;
                bus_capacity =
                    bus_capacity.wrapping_mul(CFACTOR_PERCENT as libc::c_int
                                                  as
                                                  libc::c_ulonglong).wrapping_div(100
                                                                                      as
                                                                                      libc::c_int
                                                                                      as
                                                                                      libc::c_ulonglong);
                bus_capacity =
                    (bus_capacity as
                         libc::c_ulonglong).wrapping_mul(PHYSICAL_PAGE_SIZE as
                                                             libc::c_int as
                                                             libc::c_ulonglong)
                        as u64_0 as u64_0;
                (*ptr_vdevice).logical_capacity =
                    (NUM_BUSES as libc::c_int as
                         libc::c_ulonglong).wrapping_mul(bus_capacity);
                bus_loop = 0 as libc::c_int as u64_0;
                while bus_loop < NUM_BUSES as libc::c_int as libc::c_ulonglong
                      {
                    chip_loop = 0 as libc::c_int as u64_0;
                    while chip_loop <
                              NUM_CHIPS_PER_BUS as libc::c_int as
                                  libc::c_ulonglong {
                        block_loop = 0 as libc::c_int as u64_0;
                        while block_loop <
                                  NUM_BLOCKS_PER_CHIP as libc::c_int as
                                      libc::c_ulonglong {
                            page_loop = 0 as libc::c_int as u64_0;
                            while page_loop <
                                      NUM_PAGES_PER_BLOCK as libc::c_int as
                                          libc::c_ulonglong {
                                (*ptr_vdevice).buses[bus_loop as
                                                         usize].chips[chip_loop
                                                                          as
                                                                          usize].blocks[block_loop
                                                                                            as
                                                                                            usize].pages[page_loop
                                                                                                             as
                                                                                                             usize].ptr_data
                                    =
                                    (*ptr_vdevice).ptr_vdisk[bus_loop as
                                                                 usize].offset(chip_loop.wrapping_mul(NUM_BLOCKS_PER_CHIP
                                                                                                          as
                                                                                                          libc::c_int
                                                                                                          as
                                                                                                          libc::c_ulonglong).wrapping_mul(NUM_PAGES_PER_BLOCK
                                                                                                                                              as
                                                                                                                                              libc::c_int
                                                                                                                                              as
                                                                                                                                              libc::c_ulonglong).wrapping_add(block_loop.wrapping_mul(NUM_PAGES_PER_BLOCK
                                                                                                                                                                                                          as
                                                                                                                                                                                                          libc::c_int
                                                                                                                                                                                                          as
                                                                                                                                                                                                          libc::c_ulonglong)).wrapping_add(page_loop).wrapping_mul(PHYSICAL_PAGE_SIZE
                                                                                                                                                                                                                                                                       as
                                                                                                                                                                                                                                                                       libc::c_int
                                                                                                                                                                                                                                                                       as
                                                                                                                                                                                                                                                                       libc::c_ulonglong)
                                                                                   as
                                                                                   isize);
                                page_loop = page_loop.wrapping_add(1)
                            }
                            block_loop = block_loop.wrapping_add(1)
                        }
                        chip_loop = chip_loop.wrapping_add(1)
                    }
                    bus_loop = bus_loop.wrapping_add(1)
                }
                if !(fb_bus_controller_init(ptr_vdevice,
                                            NUM_MAX_ENTRIES_OPR_QUEUE as
                                                libc::c_int as u32_0) ==
                         -(1 as libc::c_int)) {
                    return ptr_vdevice
                }
            }
            _ => { }
        }
        if !ptr_vdevice.is_null() {
            vfree(ptr_vdevice as *const libc::c_void);
        }
    }
    return 0 as *mut vdevice_t;
}
#[no_mangle]
pub unsafe extern "C" fn destroy_vdevice(mut ptr_vdevice: *mut vdevice_t) {
    let mut loop_bus: u32_0 = 0;
    if !ptr_vdevice.is_null() {
        fb_bus_controller_destroy((*ptr_vdevice).ptr_bus_controller);
        loop_bus = 0 as libc::c_int as u32_0;
        while loop_bus < NUM_BUSES as libc::c_int as libc::c_uint {
            if !(*ptr_vdevice).ptr_vdisk[loop_bus as usize].is_null() {
                vfree((*ptr_vdevice).ptr_vdisk[loop_bus as usize] as
                          *const libc::c_void);
            }
            loop_bus = loop_bus.wrapping_add(1)
        }
        vfree(ptr_vdevice as *const libc::c_void);
    };
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_read(mut ptr_vdevice: *mut vdevice_t,
                                      mut bus: u8_0, mut chip: u8_0,
                                      mut block: u32_0, mut page: u32_0,
                                      mut page_bitmap: *mut u8_0,
                                      mut ptr_dest: *mut u8_0,
                                      mut ptr_fb_bio: *mut fb_bio_t) {
    let mut ptr_src: *mut u8_0 =
        (*ptr_vdevice).buses[bus as
                                 usize].chips[chip as
                                                  usize].blocks[block as
                                                                    usize].pages[page
                                                                                     as
                                                                                     usize].ptr_data;
    let mut lp_loop: u8_0 = 0;
    let mut ptr_curr: *mut u8_0 = ptr_dest;
    lp_loop = 0 as libc::c_int as u8_0;
    while (lp_loop as libc::c_int) < NR_LP_IN_PP as libc::c_int {
        if *page_bitmap.offset(lp_loop as isize) as libc::c_int ==
               1 as libc::c_int {
            memcpy(ptr_curr as *mut libc::c_void,
                   ptr_src as *const libc::c_void,
                   LOGICAL_PAGE_SIZE as libc::c_int as libc::c_ulong);
            ptr_curr =
                ptr_curr.offset(LOGICAL_PAGE_SIZE as libc::c_int as isize)
        }
        ptr_src = ptr_src.offset(LOGICAL_PAGE_SIZE as libc::c_int as isize);
        lp_loop = lp_loop.wrapping_add(1)
    }
    fb_issue_operation(*(*ptr_vdevice).ptr_bus_controller.offset(bus as
                                                                     isize),
                       chip as u32_0, OP_READ as libc::c_int as u32_0,
                       ptr_fb_bio);
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_write(mut ptr_vdevice: *mut vdevice_t,
                                       mut bus: u8_0, mut chip: u8_0,
                                       mut block: u32_0, mut page: u32_0,
                                       mut ptr_src: *const u8_0,
                                       mut ptr_fb_bio: *mut fb_bio_t) {
    let mut ptr_dest: *mut u8_0 =
        (*ptr_vdevice).buses[bus as
                                 usize].chips[chip as
                                                  usize].blocks[block as
                                                                    usize].pages[page
                                                                                     as
                                                                                     usize].ptr_data;
    memcpy(ptr_dest as *mut libc::c_void, ptr_src as *const libc::c_void,
           PHYSICAL_PAGE_SIZE as libc::c_int as libc::c_ulong);
    fb_issue_operation(*(*ptr_vdevice).ptr_bus_controller.offset(bus as
                                                                     isize),
                       chip as u32_0, OP_PROG as libc::c_int as u32_0,
                       ptr_fb_bio);
}
#[no_mangle]
pub unsafe extern "C" fn vdevice_erase(mut ptr_vdevice: *mut vdevice_t,
                                       mut bus: u8_0, mut chip: u8_0,
                                       mut block: u32_0,
                                       mut ptr_fb_bio: *mut fb_bio_t) {
    let mut ptr_dest: *mut u8_0 =
        (*ptr_vdevice).buses[bus as
                                 usize].chips[chip as
                                                  usize].blocks[block as
                                                                    usize].pages[0
                                                                                     as
                                                                                     libc::c_int
                                                                                     as
                                                                                     usize].ptr_data;
    memset(ptr_dest as *mut libc::c_void, 0xff as libc::c_int,
           (NUM_PAGES_PER_BLOCK as libc::c_int *
                PHYSICAL_PAGE_SIZE as libc::c_int) as libc::c_ulong);
    fb_issue_operation(*(*ptr_vdevice).ptr_bus_controller.offset(bus as
                                                                     isize),
                       chip as u32_0, OP_BERS as libc::c_int as u32_0,
                       ptr_fb_bio);
}
#[no_mangle]
pub unsafe extern "C" fn is_valid_address_range(mut logical_page_address:
                                                    u32_0) -> libc::c_int {
    return (logical_page_address <
                NUM_LOG_PAGES as libc::c_int as libc::c_uint) as libc::c_int;
}
#[no_mangle]
pub unsafe extern "C" fn convert_to_physical_address(mut bus: u32_0,
                                                     mut chip: u32_0,
                                                     mut block: u32_0,
                                                     mut page: u32_0)
 -> u32_0 {
    // return (bus << 27) | (chip << 21) | (block << 8) | page;
  // Max buses: 16 - 4
  // Max chips: 16 - 4
  // Max blocks: 4096 - 12
  // Max pages: 4096 - 12
    return bus << 28 as libc::c_int | chip << 24 as libc::c_int |
               block << 12 as libc::c_int | page;
}
#[no_mangle]
pub unsafe extern "C" fn convert_to_ssd_layout(mut logical_page_address:
                                                   u32_0,
                                               mut ptr_bus: *mut u32_0,
                                               mut ptr_chip: *mut u32_0,
                                               mut ptr_block: *mut u32_0,
                                               mut ptr_page: *mut u32_0) {
    *ptr_bus =
        0xf as libc::c_int as libc::c_uint &
            logical_page_address >> 28 as libc::c_int;
    *ptr_chip =
        0xf as libc::c_int as libc::c_uint &
            logical_page_address >> 24 as libc::c_int;
    *ptr_block =
        0xfff as libc::c_int as libc::c_uint &
            logical_page_address >> 12 as libc::c_int;
    *ptr_page = 0xfff as libc::c_int as libc::c_uint & logical_page_address;
}
#[no_mangle]
pub unsafe extern "C" fn operation_time(mut op: fb_dev_op_t) -> u32_0 {
    match op as libc::c_uint {
        0 => { return TREAD as libc::c_int as u32_0 }
        1 => { return TPROG as libc::c_int as u32_0 }
        2 => { return TPLOCK as libc::c_int as u32_0 }
        3 => { return TBLOCK as libc::c_int as u32_0 }
        4 => { return TBERS as libc::c_int as u32_0 }
        _ => { return 0 as libc::c_int as u32_0 }
    };
}
