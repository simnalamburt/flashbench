use core::ffi::c_void;
use crate::constants::*;
use crate::structs::*;
use crate::linux::*;

extern {
    fn get_gcm(ftl: *mut page_mapping_context_t) -> *mut fb_gc_mngr_t;
    fn print_blk_mgmt(fb: *mut fb_context_t);
    fn get_ssd_inf(fb: *mut fb_context_t) -> *mut ssd_info;
    fn get_vdev(fb: *mut fb_context_t) -> *mut vdevice_t;
    fn get_ftl(fb: *mut fb_context_t) -> *mut c_void;
    fn set_prev_bus_chip(ptr_fb_context: *mut fb_context_t, bus: u8, chip: u8);
    fn get_next_bus_chip(
        ptr_fb_context: *mut fb_context_t,
        ptr_bus: *mut u8,
        ptr_chip: *mut u8,
    );
    fn get_curr_gc_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
    ) -> *mut flash_block;
    fn set_curr_gc_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
        ptr_new_block: *mut flash_block,
    );
    fn get_curr_active_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
    ) -> *mut flash_block;
    fn set_curr_active_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
        ptr_new_block: *mut flash_block,
    );
    fn alloc_new_page(
        ptr_fb_context: *mut fb_context_t,
        bus: u8,
        chip: u8,
        ptr_block: *mut u32,
        ptr_page: *mut u32,
    ) -> i32;
    fn map_logical_to_physical(
        ptr_fb_context: *mut fb_context_t,
        logical_page_address: *mut u32,
        bus: u32,
        chip: u32,
        block: u32,
        page: u32,
    ) -> i32;
    fn update_act_blk(fb: *mut fb_context_t, bus: u8, chip: u8);
    // page info interface
    fn get_mapped_lpa(pgi: *mut flash_page, ofs: u8) -> u32;
    fn get_pg_status(pgi: *mut flash_page, ofs: u8) -> fb_pg_status_t;
    fn get_nr_invalid_lps(pgi: *mut flash_page) -> u32;
    // block info interface
    fn init_blk_inf(blki: *mut flash_block);
    fn get_blk_idx(blki: *mut flash_block) -> u32;
    fn get_pgi_from_blki(blki: *mut flash_block, pg: u32) -> *mut flash_page;
    fn get_nr_valid_lps_in_blk(blki: *mut flash_block) -> u32;
    fn get_nr_invalid_lps_in_blk(blki: *mut flash_block) -> u32;
    fn inc_bers_cnt(blki: *mut flash_block) -> u32;
    fn set_rsv_blk_flag(blki: *mut flash_block, flag: i32);
    // chip info interface
    fn set_free_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    fn reset_free_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    fn get_free_block(ssdi: *mut ssd_info, bus: u32, chip: u32) -> *mut flash_block;
    fn get_nr_free_blks_in_chip(ci: *mut flash_chip) -> u32;
    fn get_used_block(ssdi: *mut ssd_info, bus: u32, chip: u32) -> *mut flash_block;
    fn reset_dirt_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    fn get_dirt_block(ssdi: *mut ssd_info, bus: u32, chip: u32) -> *mut flash_block;
    fn get_nr_dirt_blks_in_chip(ci: *mut flash_chip) -> u32;
    fn get_chip_info(ptr_ssd_info: *mut ssd_info, bus: u32, chip: u32) -> *mut flash_chip;
    fn perf_inc_nr_wordline_prog_bg();
    fn perf_inc_nr_page_reads();
    fn perf_inc_nr_blk_erasures();
    fn vdevice_read(
        ptr_vdevice: *mut vdevice_t,
        bus: u8,
        chip: u8,
        block: u32,
        page: u32,
        page_bitmap: *mut u8,
        ptr_dest: *mut u8,
        ptr_fb_bio: *mut fb_bio_t,
    );
    fn vdevice_write(
        ptr_vdevice: *mut vdevice_t,
        bus: u8,
        chip: u8,
        block: u32,
        page: u32,
        ptr_src: *const u8,
        ptr_fb_bio: *mut fb_bio_t,
    );
    fn vdevice_erase(
        ptr_vdevice: *mut vdevice_t,
        bus: u8,
        chip: u8,
        block: u32,
        ptr_fb_bio: *mut fb_bio_t,
    );
}
#[no_mangle]
pub unsafe extern fn init_gcm(mut gcm: *mut fb_gc_mngr_t) {
    (*gcm).nr_pgs_to_copy = 0 as i32 as u32;
}
unsafe extern fn get_vic_blk(
    gcm: *mut fb_gc_mngr_t,
    bus: u32,
    chip: u32,
) -> *mut flash_block {
    return *(*gcm).vic_blks.offset(
        bus.wrapping_mul(NUM_CHIPS_PER_BUS as i32 as u32)
            .wrapping_add(chip) as isize,
    );
}
unsafe extern fn set_vic_blk(
    gcm: *mut fb_gc_mngr_t,
    bus: u32,
    chip: u32,
    blki: *mut flash_block,
) {
    let ref mut fresh0 = *(*gcm).vic_blks.offset(
        bus.wrapping_mul(NUM_CHIPS_PER_BUS as i32 as u32)
            .wrapping_add(chip) as isize,
    );
    *fresh0 = blki;
}
unsafe extern fn find_first_valid_pg(blki: *mut flash_block, start_pg: u32) -> u32 {
    let mut pgi: *mut flash_page;
    let mut pg: u32;
    pg = start_pg;
    while pg < NUM_PAGES_PER_BLOCK as i32 as u32 {
        pgi = get_pgi_from_blki(blki, pg);
        if get_nr_invalid_lps(pgi) < NR_LP_IN_PP as i32 as u32 {
            break;
        }
        pg = pg.wrapping_add(1)
    }
    return pg;
}
unsafe extern fn set_first_valid_pg(
    gcm: *mut fb_gc_mngr_t,
    bus: u32,
    chip: u32,
    pg: u32,
) {
    *(*gcm).first_valid_pg.offset(
        bus.wrapping_mul(NUM_CHIPS_PER_BUS as i32 as u32)
            .wrapping_add(chip) as isize,
    ) = pg;
}
unsafe extern fn get_first_valid_pg(
    gcm: *mut fb_gc_mngr_t,
    bus: u32,
    chip: u32,
) -> u32 {
    return *(*gcm).first_valid_pg.offset(
        bus.wrapping_mul(NUM_CHIPS_PER_BUS as i32 as u32)
            .wrapping_add(chip) as isize,
    );
}
unsafe extern fn select_vic_blk_from_used(
    ssdi: *mut ssd_info,
    bus: u32,
    chip: u32,
) -> *mut flash_block {
    let mut vic_blki: *mut flash_block = 0 as *mut flash_block;
    let mut blki: *mut flash_block;
    let mut nr_max_invalid_lpgs: u32 = 0 as i32 as u32;
    let mut nr_invalid_lpgs: u32;
    blki = get_used_block(ssdi, bus, chip);
    while !blki.is_null() {
        nr_invalid_lpgs = get_nr_invalid_lps_in_blk(blki);
        if nr_invalid_lpgs > nr_max_invalid_lpgs {
            nr_max_invalid_lpgs = nr_invalid_lpgs;
            vic_blki = blki
        }
        blki = (*blki).next
    }
    return vic_blki;
}
unsafe extern fn select_vic_blk_greedy(
    ssdi: *mut ssd_info,
    bus: u32,
    chip: u32,
) -> *mut flash_block {
    let mut vic_blki: *mut flash_block;
    let mut blki: *mut flash_block;
    let mut nr_max_invalid_lpgs: u32 = 0 as i32 as u32;
    let mut nr_invalid_lpgs: u32;
    vic_blki = get_dirt_block(ssdi, bus, chip);
    if !vic_blki.is_null() {
        return vic_blki;
    }
    blki = get_used_block(ssdi, bus, chip);
    while !blki.is_null() {
        nr_invalid_lpgs = get_nr_invalid_lps_in_blk(blki);
        if nr_invalid_lpgs > nr_max_invalid_lpgs {
            nr_max_invalid_lpgs = nr_invalid_lpgs;
            vic_blki = blki
        }
        blki = (*blki).next
    }
    return vic_blki;
}
unsafe extern fn set_vic_blks(fb: *mut fb_context_t) -> i32 {
    let ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let mut gcm: *mut fb_gc_mngr_t = get_gcm(ftl);
    let ssdi: *mut ssd_info = get_ssd_inf(fb);
    let mut blki: *mut flash_block;
    let mut bus: u32;
    let mut chip: u32;
    bus = 0 as i32 as u32;
    while bus < NUM_BUSES as i32 as u32 {
        chip = 0 as i32 as u32;
        while chip < NUM_CHIPS_PER_BUS as i32 as u32 {
            if !get_curr_gc_block(fb, bus, chip).is_null()
                && !get_curr_active_block(fb, bus, chip).is_null()
            {
                blki = 0 as *mut flash_block
            } else {
                blki = select_vic_blk_greedy(ssdi, bus, chip);
                if blki.is_null() {
                    printk(b"\x013flashbench: gc_page_mapping: There is no avaiable victim block.\n\x00"
                               as *const u8 as *const i8);
                    return -(1 as i32);
                }
                if get_nr_valid_lps_in_blk(blki) == 0 as i32 as u32 {
                    blki = 0 as *mut flash_block
                } else {
                    (*gcm).nr_pgs_to_copy = ((*gcm).nr_pgs_to_copy as u32)
                        .wrapping_add(get_nr_valid_lps_in_blk(blki))
                        as u32 as u32;
                    set_first_valid_pg(
                        gcm,
                        bus,
                        chip,
                        find_first_valid_pg(blki, 0 as i32 as u32),
                    );
                }
            }
            set_vic_blk(gcm, bus, chip, blki);
            chip = chip.wrapping_add(1)
        }
        bus = bus.wrapping_add(1)
    }
    return 0 as i32;
}
unsafe extern fn get_valid_pgs_in_vic_blks(fb: *mut fb_context_t) {
    let ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let gcm: *mut fb_gc_mngr_t = get_gcm(ftl);
    let vdev: *mut vdevice_t = get_vdev(fb);
    let mut blki: *mut flash_block;
    let mut pgi: *mut flash_page;
    let mut nr_read_pgs: u32 = 0 as i32 as u32;
    let mut ptr_lpa: *mut u32 = (*gcm).lpas_to_copy;
    let mut ptr_data: *mut u8 = (*gcm).data_to_copy;
    let mut lp_bitmap: [u8; 4] = [0; 4];
    let mut bus: u8;
    let mut chip: u8;
    let mut pg: u32;
    let mut lp: u32;
    let mut nr_pgs_to_read;
    while (*gcm).nr_pgs_to_copy > nr_read_pgs {
        pg = 0 as i32 as u32;
        while pg < NUM_PAGES_PER_BLOCK as i32 as u32 {
            chip = 0 as i32 as u8;
            while (chip as i32) < NUM_CHIPS_PER_BUS as i32 {
                bus = 0 as i32 as u8;
                while (bus as i32) < NUM_BUSES as i32 {
                    blki = get_vic_blk(gcm, bus as u32, chip as u32);
                    if !blki.is_null() {
                        pgi = get_pgi_from_blki(blki, pg);
                        nr_pgs_to_read = (NR_LP_IN_PP as i32 as u32)
                            .wrapping_sub(get_nr_invalid_lps(pgi));
                        if nr_pgs_to_read > 0 as i32 as u32 {
                            lp = 0 as i32 as u32;
                            while lp < NR_LP_IN_PP as i32 as u32 {
                                if get_pg_status(pgi, lp as u8) as u32
                                    == PAGE_STATUS_VALID as i32 as u32
                                {
                                    *ptr_lpa = get_mapped_lpa(pgi, lp as u8);
                                    lp_bitmap[lp as usize] = 1 as i32 as u8;
                                    ptr_lpa = ptr_lpa.offset(1)
                                } else {
                                    lp_bitmap[lp as usize] = 0 as i32 as u8
                                }
                                lp = lp.wrapping_add(1)
                            }
                            perf_inc_nr_page_reads();
                            vdevice_read(
                                vdev,
                                bus,
                                chip,
                                get_blk_idx(blki),
                                pg,
                                lp_bitmap.as_mut_ptr(),
                                ptr_data,
                                0 as *mut fb_bio_t,
                            );
                            ptr_data = ptr_data.offset(
                                nr_pgs_to_read
                                    .wrapping_mul(LOGICAL_PAGE_SIZE as i32 as u32)
                                    as isize,
                            );
                            nr_read_pgs = (nr_read_pgs as u32).wrapping_add(nr_pgs_to_read)
                                as u32 as u32
                        }
                    }
                    bus = bus.wrapping_add(1)
                }
                chip = chip.wrapping_add(1)
            }
            pg = pg.wrapping_add(1)
        }
    }
}
#[no_mangle]
pub unsafe extern fn prog_valid_pgs_to_gc_blks(fb: *mut fb_context_t) -> i32 {
    let ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let gcm: *mut fb_gc_mngr_t = get_gcm(ftl);
    let vdev: *mut vdevice_t = get_vdev(fb);
    let mut nr_pgs_to_prog: i32 = (*gcm).nr_pgs_to_copy as i32;
    let mut ptr_lpa: *mut u32 = (*gcm).lpas_to_copy;
    let mut ptr_data: *mut u8 = (*gcm).data_to_copy;
    let mut bus: u8 = 0;
    let mut chip: u8 = 0;
    let mut blk: u32 = 0;
    let mut pg: u32 = 0;
    let mut lp: u32;
    while nr_pgs_to_prog > 0 as i32 {
        get_next_bus_chip(fb, &mut bus, &mut chip);
        if alloc_new_page(fb, bus, chip, &mut blk, &mut pg) == -(1 as i32) {
            printk(
                b"\x013flashbench: Wrong active block handling\n\x00" as *const u8
                    as *const i8,
            );
            print_blk_mgmt(fb);
            return -(1 as i32);
        }
        if nr_pgs_to_prog < NR_LP_IN_PP as i32 {
            lp = nr_pgs_to_prog as u32;
            while lp < NR_LP_IN_PP as i32 as u32 {
                *ptr_lpa.offset(lp as isize) = PAGE_UNMAPPED as i32 as u32;
                lp = lp.wrapping_add(1)
            }
        }
        perf_inc_nr_wordline_prog_bg();
        vdevice_write(vdev, bus, chip, blk, pg, ptr_data, 0 as *mut fb_bio_t);
        if map_logical_to_physical(fb, ptr_lpa, bus as u32, chip as u32, blk, pg)
            == -(1 as i32)
        {
            printk(
                b"\x013flashbench: Mapping L2P in GC failed.\n\x00" as *const u8
                    as *const i8,
            );
            return -(1 as i32);
        }
        ptr_lpa = ptr_lpa.offset(NR_LP_IN_PP as i32 as isize);
        ptr_data = ptr_data.offset(PHYSICAL_PAGE_SIZE as i32 as isize);
        nr_pgs_to_prog -= NR_LP_IN_PP as i32;
        set_prev_bus_chip(fb, bus, chip);
        update_act_blk(fb, bus, chip);
    }
    return 0 as i32;
}
unsafe extern fn prepare_act_blks(fb: *mut fb_context_t) -> i32 {
    let vdev: *mut vdevice_t = get_vdev(fb);
    let mut blki: *mut flash_block;
    let mut bus: u8;
    let mut chip: u8;
    chip = 0 as i32 as u8;
    while (chip as i32) < NUM_CHIPS_PER_BUS as i32 {
        bus = 0 as i32 as u8;
        while (bus as i32) < NUM_BUSES as i32 {
            if get_curr_active_block(fb, bus as u32, chip as u32).is_null() {
                blki = get_curr_gc_block(fb, bus as u32, chip as u32);
                if blki.is_null() {
                    printk(
                        b"\x013flashbench: Wrong GC block handling\n\x00" as *const u8
                            as *const i8,
                    );
                    print_blk_mgmt(fb);
                    return -(1 as i32);
                }
                vdevice_erase(vdev, bus, chip, get_blk_idx(blki), 0 as *mut fb_bio_t);
                perf_inc_nr_blk_erasures();
                init_blk_inf(blki);
                inc_bers_cnt(blki);
                set_curr_active_block(fb, bus as u32, chip as u32, blki);
                set_curr_gc_block(fb, bus as u32, chip as u32, 0 as *mut flash_block);
            }
            bus = bus.wrapping_add(1)
        }
        chip = chip.wrapping_add(1)
    }
    return 0 as i32;
}
#[no_mangle]
pub unsafe extern fn update_gc_blks(fb: *mut fb_context_t) -> i32 {
    let mut gc_blki: *mut flash_block;
    let mut bus: u32;
    let mut chip: u32;
    bus = 0 as i32 as u32;
    while bus < NUM_BUSES as i32 as u32 {
        chip = 0 as i32 as u32;
        while chip < NUM_CHIPS_PER_BUS as i32 as u32 {
            if get_curr_gc_block(fb, bus, chip).is_null() {
                gc_blki = get_dirt_block(get_ssd_inf(fb), bus, chip);
                if gc_blki.is_null() {
                    if get_curr_active_block(fb, bus, chip).is_null() {
                        printk(
                            b"\x013flashbench: Wrong block management handling\n\x00" as *const u8
                                as *const i8,
                        );
                        print_blk_mgmt(fb);
                        return -(1 as i32);
                    }
                } else {
                    reset_dirt_blk(get_ssd_inf(fb), gc_blki);
                    set_curr_gc_block(fb, bus, chip, gc_blki);
                }
            }
            chip = chip.wrapping_add(1)
        }
        bus = bus.wrapping_add(1)
    }
    return 0 as i32;
}
#[no_mangle]
pub unsafe extern fn create_gc_mngr(fb: *mut fb_context_t) -> *mut fb_gc_mngr_t {
    let current_block: u64;
    let ssdi: *mut ssd_info = get_ssd_inf(fb);
    let mut gcm: *mut fb_gc_mngr_t;
    let mut blki: *mut flash_block;
    let mut bus: u32;
    let mut chip: u32;
    gcm = vmalloc(::core::mem::size_of::<fb_gc_mngr_t>() as u64) as *mut fb_gc_mngr_t;
    if gcm.is_null() {
        printk(
            b"\x013flashbench: Allocating GC manager failed.\n\x00" as *const u8
                as *const i8,
        );
    } else {
        (*gcm).gc_blks = vmalloc(
            (::core::mem::size_of::<*mut flash_block>() as u64)
                .wrapping_mul(NUM_CHIPS as i32 as u64),
        ) as *mut *mut flash_block;
        if (*gcm).gc_blks.is_null() {
            printk(
                b"\x013flashbench: Allocating GC block list failed.\n\x00" as *const u8
                    as *const i8,
            );
        } else {
            (*gcm).vic_blks = vmalloc(
                (::core::mem::size_of::<*mut flash_block>() as u64)
                    .wrapping_mul(NUM_CHIPS as i32 as u64),
            ) as *mut *mut flash_block;
            if (*gcm).vic_blks.is_null() {
                printk(
                    b"\x013flashbench: Allocating victim block list failed.\n\x00" as *const u8
                        as *const i8,
                );
            } else {
                (*gcm).lpas_to_copy = vmalloc(
                    (::core::mem::size_of::<u32>() as u64)
                        .wrapping_mul(NUM_CHIPS as i32 as u64)
                        .wrapping_mul(NUM_PAGES_PER_BLOCK as i32 as u64)
                        .wrapping_mul(NR_LP_IN_PP as i32 as u64),
                ) as *mut u32;
                if (*gcm).lpas_to_copy.is_null() {
                    printk(
                        b"\x013flashbench: Allocating LPA list failed.\n\x00" as *const u8
                            as *const i8,
                    );
                } else {
                    (*gcm).data_to_copy = vmalloc(
                        (::core::mem::size_of::<u32>() as u64)
                            .wrapping_mul(NUM_CHIPS as i32 as u64)
                            .wrapping_mul(NUM_PAGES_PER_BLOCK as i32 as u64)
                            .wrapping_mul(NR_LP_IN_PP as i32 as u64)
                            .wrapping_mul(LOGICAL_PAGE_SIZE as i32 as u64),
                    ) as *mut u8;
                    if (*gcm).data_to_copy.is_null() {
                        printk(
                            b"\x013flashbench: Allocating valid page buffer failed.\n\x00"
                                as *const u8 as *const i8,
                        );
                    } else {
                        (*gcm).first_valid_pg = vmalloc(
                            (::core::mem::size_of::<u32>() as u64)
                                .wrapping_mul(NUM_CHIPS as i32 as u64),
                        ) as *mut u32;
                        if (*gcm).first_valid_pg.is_null() {
                            printk(
                                b"\x013flashbench: Allocating page_offset failed.\n\x00"
                                    as *const u8
                                    as *const i8,
                            );
                        } else {
                            init_gcm(gcm);
                            bus = 0 as i32 as u32;
                            's_97: loop {
                                if !(bus < NUM_BUSES as i32 as u32) {
                                    current_block = 4775909272756257391;
                                    break;
                                }
                                chip = 0 as i32 as u32;
                                while chip < NUM_CHIPS_PER_BUS as i32 as u32 {
                                    blki = get_free_block(ssdi, bus, chip);
                                    if blki.is_null() {
                                        printk(
                                            b"\x013flashbench: Getting new gc block failed.\n\x00"
                                                as *const u8
                                                as *const i8,
                                        );
                                        current_block = 8921050815587899969;
                                        break 's_97;
                                    } else {
                                        reset_free_blk(ssdi, blki);
                                        set_rsv_blk_flag(blki, true as i32);
                                        let ref mut fresh1 = *(*gcm).gc_blks.offset(
                                            bus.wrapping_mul(
                                                NUM_CHIPS_PER_BUS as i32 as u32,
                                            )
                                            .wrapping_add(chip)
                                                as isize,
                                        );
                                        *fresh1 = blki;
                                        chip = chip.wrapping_add(1)
                                    }
                                }
                                bus = bus.wrapping_add(1)
                            }
                            match current_block {
                                8921050815587899969 => {}
                                _ => return gcm,
                            }
                        }
                    }
                }
            }
        }
    }
    destroy_gc_mngr(gcm);
    return 0 as *mut fb_gc_mngr_t;
}
#[no_mangle]
pub unsafe extern fn destroy_gc_mngr(gcm: *mut fb_gc_mngr_t) {
    if !gcm.is_null() {
        if !(*gcm).gc_blks.is_null() {
            vfree((*gcm).gc_blks as *const c_void);
        }
        if !(*gcm).vic_blks.is_null() {
            vfree((*gcm).vic_blks as *const c_void);
        }
        if !(*gcm).lpas_to_copy.is_null() {
            vfree((*gcm).lpas_to_copy as *const c_void);
        }
        if !(*gcm).data_to_copy.is_null() {
            vfree((*gcm).data_to_copy as *const c_void);
        }
        vfree(gcm as *const c_void);
    };
}
#[no_mangle]
pub unsafe extern fn trigger_gc_page_mapping(fb: *mut fb_context_t) -> i32 {
    let ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let gcm: *mut fb_gc_mngr_t = get_gcm(ftl);
    // initialize GC context
    init_gcm(gcm);
    // 1. erase GC blocks and set them as active blocks
    if prepare_act_blks(fb) != 0 as i32 {
        printk(
            b"\x013flashbench: Preparing GC blocks failed.\n\x00" as *const u8
                as *const i8,
        );
        return -(1 as i32);
    }
    // 2. select a victim block for every parallel unit
    if set_vic_blks(fb) != 0 as i32 {
        printk(
            b"\x013flashbench: Setting victim blocks failed.\n\x00" as *const u8
                as *const i8,
        );
        return -(1 as i32);
    }
    // 3. read valid pages from victim blocks
    // JS - here is the function
    get_valid_pgs_in_vic_blks(fb);
    // 4. write read data to other pages
    if prog_valid_pgs_to_gc_blks(fb) != 0 as i32 {
        printk(
            b"\x013flashbench: Writing valid data failed.\n\x00" as *const u8
                as *const i8,
        );
        return -(1 as i32);
    }
    // 5. set GC blocks with dirt blocks
    if update_gc_blks(fb) != 0 as i32 {
        printk(
            b"\x013flashbench: Updating GC blocks failed.\n\x00" as *const u8
                as *const i8,
        );
        return -(1 as i32);
    }
    return 0 as i32;
}
#[no_mangle]
pub unsafe extern fn fb_bgc_prepare_act_blks(fb: *mut fb_context_t) -> i32 {
    let ssdi: *mut ssd_info = get_ssd_inf(fb);
    let mut blki: *mut flash_block;
    let mut bus: u8 = 0;
    let mut chip: u8 = 0;
    let mut i: u32;
    i = 0 as i32 as u32;
    while i < NUM_CHIPS as i32 as u32 {
        get_next_bus_chip(fb, &mut bus, &mut chip);
        blki = get_curr_gc_block(fb, bus as u32, chip as u32);
        if blki.is_null() {
            printk(
                b"\x013flashbench: Wrong BGC block handling\n\x00" as *const u8
                    as *const i8,
            );
            print_blk_mgmt(fb);
            return -(1 as i32);
        }
        vdevice_erase(
            get_vdev(fb),
            bus,
            chip,
            get_blk_idx(blki),
            0 as *mut fb_bio_t,
        );
        perf_inc_nr_blk_erasures();
        init_blk_inf(blki);
        inc_bers_cnt(blki);
        if get_curr_active_block(fb, bus as u32, chip as u32).is_null() {
            set_curr_active_block(fb, bus as u32, chip as u32, blki);
        } else {
            set_free_blk(ssdi, blki);
        }
        blki = get_dirt_block(ssdi, bus as u32, chip as u32);
        if !blki.is_null() {
            reset_dirt_blk(ssdi, blki);
        }
        set_curr_gc_block(fb, bus as u32, chip as u32, blki);
        set_prev_bus_chip(fb, bus, chip);
        i = i.wrapping_add(1)
    }
    return 0 as i32;
}
#[no_mangle]
pub unsafe extern fn fb_bgc_set_vic_blks(fb: *mut fb_context_t) -> i32 {
    let ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let ssdi: *mut ssd_info = get_ssd_inf(fb);
    let gcm: *mut fb_gc_mngr_t = get_gcm(ftl);
    let mut chipi: *mut flash_chip;
    let mut blki: *mut flash_block;
    let mut bus: u8;
    let mut chip: u8;
    // if necesary, find a new victim block for each chip
    bus = 0 as i32 as u8;
    while (bus as i32) < NUM_BUSES as i32 {
        let mut current_block_15: u64;
        chip = 0 as i32 as u8;
        while (chip as i32) < NUM_CHIPS_PER_BUS as i32 {
            chipi = get_chip_info(ssdi, bus as u32, chip as u32);
            if get_nr_dirt_blks_in_chip(chipi).wrapping_add(get_nr_free_blks_in_chip(chipi))
                < BGC_TH_NR_BLKS as i32 as u32
            {
                // victim block exists
                blki = get_vic_blk(gcm, bus as u32, chip as u32);
                if !blki.is_null() {
                    set_first_valid_pg(
                        gcm,
                        bus as u32,
                        chip as u32,
                        find_first_valid_pg(
                            blki,
                            get_first_valid_pg(gcm, bus as u32, chip as u32),
                        ),
                    );
                    if get_first_valid_pg(gcm, bus as u32, chip as u32)
                        == NUM_PAGES_PER_BLOCK as i32 as u32
                    {
                        set_vic_blk(gcm, bus as u32, chip as u32, 0 as *mut flash_block);
                        current_block_15 = 7976072742316086414;
                    } else {
                        current_block_15 = 6483416627284290920;
                    }
                } else {
                    current_block_15 = 7976072742316086414;
                }
                match current_block_15 {
                    6483416627284290920 => {}
                    _ => {
                        // find a new one
                        blki = select_vic_blk_from_used(ssdi, bus as u32, chip as u32);
                        if blki.is_null() {
                            printk(
                                b"\x013flashbench: Wrong block managment\n\x00" as *const u8
                                    as *const i8,
                            );
                            print_blk_mgmt(fb);
                            return -(1 as i32);
                        }
                        set_vic_blk(gcm, bus as u32, chip as u32, blki);
                        set_first_valid_pg(
                            gcm,
                            bus as u32,
                            chip as u32,
                            find_first_valid_pg(blki, 0 as i32 as u32),
                        );
                    }
                }
            } else {
                set_vic_blk(gcm, bus as u32, chip as u32, 0 as *mut flash_block);
                set_first_valid_pg(gcm, bus as u32, chip as u32, 0 as i32 as u32);
            }
            chip = chip.wrapping_add(1)
        }
        bus = bus.wrapping_add(1)
    }
    return 0 as i32;
}
#[no_mangle]
pub unsafe extern fn fb_bgc_read_valid_pgs(fb: *mut fb_context_t) -> i32 {
    let ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let mut gcm: *mut fb_gc_mngr_t = get_gcm(ftl);
    let mut vic_blki: *mut flash_block;
    let mut pgi: *mut flash_page;
    let mut bus: u8 = 0;
    let mut chip: u8 = 0;
    let mut lp: u8;
    let mut pg: u32;
    let mut nr_pgs_to_read: u32;
    let mut lp_bitmap: [u8; 4] = [0; 4];
    let mut ptr_lpas: *mut u32 = (*gcm).lpas_to_copy;
    let mut ptr_data: *mut u8 = (*gcm).data_to_copy;
    let mut i: u32;
    i = 0 as i32 as u32;
    while i < NUM_CHIPS as i32 as u32 {
        get_next_bus_chip(fb, &mut bus, &mut chip);
        vic_blki = get_vic_blk(gcm, bus as u32, chip as u32);
        if vic_blki.is_null() {
            set_prev_bus_chip(fb, bus, chip);
        } else {
            pg = get_first_valid_pg(gcm, bus as u32, chip as u32);
            pgi = get_pgi_from_blki(vic_blki, pg);
            nr_pgs_to_read =
                (NR_LP_IN_PP as i32 as u32).wrapping_sub(get_nr_invalid_lps(pgi));
            if nr_pgs_to_read == 0 as i32 as u32 {
                printk(
                    b"\x013flashbench: Wrong page offset in victim block\n\x00" as *const u8
                        as *const i8,
                );
                return -(1 as i32);
            }
            (*gcm).nr_pgs_to_copy = ((*gcm).nr_pgs_to_copy as u32)
                .wrapping_add(nr_pgs_to_read) as u32 as u32;
            lp = 0 as i32 as u8;
            while (lp as i32) < NR_LP_IN_PP as i32 {
                if get_pg_status(pgi, lp) as u32
                    == PAGE_STATUS_VALID as i32 as u32
                {
                    *ptr_lpas = get_mapped_lpa(pgi, lp);
                    lp_bitmap[lp as usize] = 1 as i32 as u8;
                    ptr_lpas = ptr_lpas.offset(1)
                } else {
                    lp_bitmap[lp as usize] = 0 as i32 as u8
                }
                lp = lp.wrapping_add(1)
            }
            perf_inc_nr_page_reads();
            vdevice_read(
                get_vdev(fb),
                bus,
                chip,
                get_blk_idx(vic_blki),
                pg,
                lp_bitmap.as_mut_ptr(),
                ptr_data,
                0 as *mut fb_bio_t,
            );
            ptr_data = ptr_data.offset(
                nr_pgs_to_read.wrapping_mul(LOGICAL_PAGE_SIZE as i32 as u32)
                    as isize,
            );
            set_prev_bus_chip(fb, bus, chip);
        }
        i = i.wrapping_add(1)
    }
    return 0 as i32;
}
