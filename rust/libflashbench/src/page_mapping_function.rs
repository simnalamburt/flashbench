#![allow(dead_code, mutable_transmutes, non_camel_case_types, non_snake_case,
         non_upper_case_globals, unused_assignments, unused_mut)]

use linux_types as libc;

extern "C" {
    pub type fb_context_t;
    pub type page_mapping_context_t;
    pub type flash_page;
    pub type ssd_info;
    #[no_mangle]
    fn set_curr_active_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32_0,
        chip: u32_0,
        ptr_new_block: *mut flash_block,
    );
    #[no_mangle]
    fn get_curr_active_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32_0,
        chip: u32_0,
    ) -> *mut flash_block;
    #[no_mangle]
    fn get_mapped_ppa(ftl: *mut page_mapping_context_t, lpa: u32_0) -> u32_0;
    #[no_mangle]
    fn set_mapped_ppa(ftl: *mut page_mapping_context_t, lpa: u32_0, ppa: u32_0);
    #[no_mangle]
    fn get_curr_gc_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32_0,
        chip: u32_0,
    ) -> *mut flash_block;
    #[no_mangle]
    fn set_curr_gc_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32_0,
        chip: u32_0,
        ptr_new_block: *mut flash_block,
    );
    #[no_mangle]
    fn get_abm(ftl: *mut page_mapping_context_t) -> *mut fb_act_blk_mngr_t;
    #[no_mangle]
    fn get_ssd_inf(fb: *mut fb_context_t) -> *mut ssd_info;
    #[no_mangle]
    fn get_ftl(fb: *mut fb_context_t) -> *mut libc::c_void;
    #[no_mangle]
    fn set_mapped_lpa(pgi: *mut flash_page, ofs: u8_0, lpa: u32_0);
    #[no_mangle]
    fn set_pg_status(pgi: *mut flash_page, ofs: u8_0, status: fb_pg_status_t);
    #[no_mangle]
    fn inc_nr_invalid_lps(pgi: *mut flash_page) -> u32_0;
    #[no_mangle]
    fn get_nr_free_pgs(blki: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn inc_nr_valid_pgs(blki: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn inc_nr_invalid_pgs(blki: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn dec_nr_valid_pgs(blki: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn dec_nr_free_pgs(blki: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn inc_nr_valid_lps_in_blk(blk: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn inc_nr_invalid_lps_in_blk(blk: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn dec_nr_valid_lps_in_blk(blk: *mut flash_block) -> u32_0;
    #[no_mangle]
    fn set_act_blk_flag(blki: *mut flash_block, flag: libc::c_int);
    #[no_mangle]
    fn reset_free_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    #[no_mangle]
    fn get_free_block(ssdi: *mut ssd_info, bus: u32_0, chip: u32_0) -> *mut flash_block;
    #[no_mangle]
    fn set_used_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    #[no_mangle]
    fn reset_used_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    #[no_mangle]
    fn set_dirt_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    #[no_mangle]
    fn get_block_info(
        ptr_ssd_info: *mut ssd_info,
        bus: u32_0,
        chip: u32_0,
        block: u32_0,
    ) -> *mut flash_block;
    #[no_mangle]
    fn get_page_info(
        ptr_ssd_info: *mut ssd_info,
        bus: u32_0,
        chip: u32_0,
        block: u32_0,
        page: u32_0,
    ) -> *mut flash_page;
    #[no_mangle]
    fn convert_to_physical_address(bus: u32_0, chip: u32_0, block: u32_0, page: u32_0) -> u32_0;
    #[no_mangle]
    fn convert_to_ssd_layout(
        logical_page_address: u32_0,
        ptr_bus: *mut u32_0,
        ptr_chip: *mut u32_0,
        ptr_block: *mut u32_0,
        ptr_page: *mut u32_0,
    );
}
pub type u8_0 = libc::c_uchar;
pub type u32_0 = libc::c_uint;
pub type C2RustUnnamed = libc::c_uint;
pub const true_0: C2RustUnnamed = 1;
pub const false_0: C2RustUnnamed = 0;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct flash_block {
    pub no_block: u32_0,
    pub no_chip: u32_0,
    pub no_bus: u32_0,
    pub nr_valid_pages: u32_0,
    pub nr_invalid_pages: u32_0,
    pub nr_free_pages: u32_0,
    pub nr_valid_log_pages: u32_0,
    pub nr_invalid_log_pages: u32_0,
    pub nr_erase_cnt: u32_0,
    pub is_bad_block: libc::c_int,
    pub last_modified_time: u32_0,
    pub is_reserved_block: libc::c_int,
    pub is_active_block: libc::c_int,
    pub list_pages: *mut flash_page,
    pub prev: *mut flash_block,
    pub next: *mut flash_block,
    pub del_flag: libc::c_int,
}
pub type C2RustUnnamed_0 = libc::c_int;
pub const PAGE_UNMAPPED: C2RustUnnamed_0 = -1;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct fb_act_blk_mngr_t {
    pub act_blks: *mut *mut flash_block,
    pub mru_bus: u32_0,
    pub mru_chip: u32_0,
}
pub type fb_pg_status_t = libc::c_uint;
pub const PAGE_STATUS_DEL: fb_pg_status_t = 3;
pub const PAGE_STATUS_INVALID: fb_pg_status_t = 2;
pub const PAGE_STATUS_VALID: fb_pg_status_t = 1;
pub const PAGE_STATUS_FREE: fb_pg_status_t = 0;
pub type C2RustUnnamed_1 = libc::c_uint;
pub const BGC_TH_NR_BLKS: C2RustUnnamed_1 = 14;
pub const BGC_TH_WB_UTIL: C2RustUnnamed_1 = 5;
pub const BGC_TH_INTV: C2RustUnnamed_1 = 5000;
pub const NUM_MAX_ENTRIES_OPR_QUEUE: C2RustUnnamed_1 = 4;
pub const TBLOCK: C2RustUnnamed_1 = 100;
pub const TPLOCK: C2RustUnnamed_1 = 100;
pub const TBERS: C2RustUnnamed_1 = 5000;
pub const TPROG: C2RustUnnamed_1 = 800;
pub const TREAD: C2RustUnnamed_1 = 80;
pub const SECTOR_SIZE: C2RustUnnamed_1 = 512;
pub const PHYSICAL_PAGE_SIZE: C2RustUnnamed_1 = 16384;
pub const LOGICAL_PAGE_SIZE: C2RustUnnamed_1 = 4096;
pub const NUM_LOG_PAGES: C2RustUnnamed_1 = 236390;
pub const NUM_PAGES_IN_WRITE_BUFFER: C2RustUnnamed_1 = 8;
pub const NUM_PAGES: C2RustUnnamed_1 = 65664;
pub const NUM_BLOCKS: C2RustUnnamed_1 = 342;
pub const NUM_CHIPS: C2RustUnnamed_1 = 2;
pub const LP_PAGE_SHIFT: C2RustUnnamed_1 = 2;
pub const LP_PAGE_MASK: C2RustUnnamed_1 = 3;
pub const NR_LP_IN_PP: C2RustUnnamed_1 = 4;
// v-layer * h-layer * multi-level degree
pub const CFACTOR_PERCENT: C2RustUnnamed_1 = 90;
pub const NUM_PAGES_PER_BLOCK: C2RustUnnamed_1 = 192;
pub const NUM_BLOCKS_PER_CHIP: C2RustUnnamed_1 = 171;
pub const NUM_CHIPS_PER_BUS: C2RustUnnamed_1 = 2;
//
// Hardware configuration
//
pub const NUM_BUSES: C2RustUnnamed_1 = 1;
pub const NR_MAX_LPGS_COPY: C2RustUnnamed_1 = 6144;
pub const NUM_BTODS: C2RustUnnamed_1 = 2048;
pub const NUM_WTODS: C2RustUnnamed_1 = 2048;
//
// Parameterf for DEL manager
//
pub const NR_MAX_LPAS_DISCARD: C2RustUnnamed_1 = 2048;
#[no_mangle]
pub unsafe extern "C" fn get_prev_bus_chip(
    mut fb: *mut fb_context_t,
    mut bus: *mut u8_0,
    mut chip: *mut u8_0,
) {
    let mut ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let mut abm: *mut fb_act_blk_mngr_t = get_abm(ftl);
    *bus = (*abm).mru_bus as u8_0;
    *chip = (*abm).mru_chip as u8_0;
}
#[no_mangle]
pub unsafe extern "C" fn alloc_new_page(
    mut fb: *mut fb_context_t,
    mut bus: u8_0,
    mut chip: u8_0,
    mut blk: *mut u32_0,
    mut pg: *mut u32_0,
) -> libc::c_int {
    let mut ssdi: *mut ssd_info = get_ssd_inf(fb);
    let mut blki: *mut flash_block = 0 as *mut flash_block;
    blki = get_curr_active_block(fb, bus as u32_0, chip as u32_0);
    if blki.is_null() {
        blki = get_free_block(ssdi, bus as u32_0, chip as u32_0);
        if blki.is_null() {
            return -(1 as libc::c_int);
        } else {
            reset_free_blk(ssdi, blki);
            set_curr_active_block(fb, bus as u32_0, chip as u32_0, blki);
        }
    }
    *blk = (*blki).no_block;
    *pg = (NUM_PAGES_PER_BLOCK as libc::c_int as libc::c_uint).wrapping_sub(get_nr_free_pgs(blki));
    *blk = (*blki).no_block;
    *pg = (NUM_PAGES_PER_BLOCK as libc::c_int as libc::c_uint).wrapping_sub(get_nr_free_pgs(blki));
    return 0 as libc::c_int;
}
#[no_mangle]
pub unsafe extern "C" fn map_logical_to_physical(
    mut ptr_fb_context: *mut fb_context_t,
    mut logical_page_address: *mut u32_0,
    mut bus: u32_0,
    mut chip: u32_0,
    mut block: u32_0,
    mut page: u32_0,
) -> libc::c_int {
    let mut lp_loop: u8_0 = 0;
    let mut ret: libc::c_int = -(1 as libc::c_int);
    lp_loop = 0 as libc::c_int as u8_0;
    while (lp_loop as libc::c_int) < NR_LP_IN_PP as libc::c_int {
        ret = __map_logical_to_physical(
            ptr_fb_context,
            *logical_page_address.offset(lp_loop as isize),
            bus,
            chip,
            block,
            page,
            lp_loop,
        );
        if ret == -(1 as libc::c_int) {
            break;
        }
        lp_loop = lp_loop.wrapping_add(1)
    }
    return ret;
}
#[no_mangle]
pub unsafe extern "C" fn update_act_blk(mut fb: *mut fb_context_t, mut bus: u8_0, mut chip: u8_0) {
    let mut blki: *mut flash_block = get_curr_active_block(fb, bus as u32_0, chip as u32_0);
    if get_nr_free_pgs(blki) == 0 as libc::c_int as libc::c_uint {
        let mut ssdi: *mut ssd_info = get_ssd_inf(fb);
        set_act_blk_flag(blki, false_0 as libc::c_int);
        set_used_blk(ssdi, blki);
        blki = get_free_block(ssdi, bus as u32_0, chip as u32_0);
        if !blki.is_null() {
            reset_free_blk(ssdi, blki);
        }
        set_curr_active_block(fb, bus as u32_0, chip as u32_0, blki);
    };
}
#[no_mangle]
pub unsafe extern "C" fn invalidate_lpg(mut fb: *mut fb_context_t, mut lpa: u32_0) -> u32_0 {
    let mut ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let mut ssdi: *mut ssd_info = get_ssd_inf(fb);
    let mut ppa: u32_0 = get_mapped_ppa(ftl, lpa);
    if ppa != PAGE_UNMAPPED as libc::c_int as u32_0 {
        let mut bus: u32_0 = 0;
        let mut chip: u32_0 = 0;
        let mut blk: u32_0 = 0;
        let mut pg: u32_0 = 0;
        let mut blki: *mut flash_block = 0 as *mut flash_block;
        let mut pgi: *mut flash_page = 0 as *mut flash_page;
        convert_to_ssd_layout(ppa, &mut bus, &mut chip, &mut blk, &mut pg);
        blki = get_block_info(ssdi, bus, chip, blk);
        pgi = get_page_info(ssdi, bus, chip, blk, pg >> LP_PAGE_SHIFT as libc::c_int);
        set_pg_status(
            pgi,
            (pg & LP_PAGE_MASK as libc::c_int as libc::c_uint) as u8_0,
            PAGE_STATUS_INVALID,
        );
        inc_nr_invalid_lps_in_blk(blki);
        dec_nr_valid_lps_in_blk(blki);
        if inc_nr_invalid_lps(pgi) == NR_LP_IN_PP as libc::c_int as libc::c_uint {
            dec_nr_valid_pgs(blki);
            if inc_nr_invalid_pgs(blki) == NUM_PAGES_PER_BLOCK as libc::c_int as libc::c_uint {
                reset_used_blk(ssdi, blki);
                if get_curr_gc_block(fb, bus, chip).is_null() {
                    set_curr_gc_block(fb, bus, chip, blki);
                } else {
                    set_dirt_blk(ssdi, blki);
                }
            }
        }
        set_mapped_ppa(ftl, lpa, PAGE_UNMAPPED as libc::c_int as u32_0);
    }
    return ppa;
}
unsafe extern "C" fn __map_logical_to_physical(
    mut fb: *mut fb_context_t,
    mut lpa: u32_0,
    mut bus: u32_0,
    mut chip: u32_0,
    mut blk: u32_0,
    mut pg: u32_0,
    mut lp_ofs: u8_0,
) -> libc::c_int {
    let mut ftl: *mut page_mapping_context_t = get_ftl(fb) as *mut page_mapping_context_t;
    let mut ssdi: *mut ssd_info = get_ssd_inf(fb);
    let mut blki: *mut flash_block = get_block_info(ssdi, bus, chip, blk);
    let mut pgi: *mut flash_page = get_page_info(ssdi, bus, chip, blk, pg);
    if lpa != PAGE_UNMAPPED as libc::c_int as u32_0 {
        invalidate_lpg(fb, lpa);
        inc_nr_valid_lps_in_blk(blki);
        set_pg_status(pgi, lp_ofs, PAGE_STATUS_VALID);
        set_mapped_ppa(
            ftl,
            lpa,
            convert_to_physical_address(
                bus,
                chip,
                blk,
                pg << LP_PAGE_SHIFT as libc::c_int | lp_ofs as libc::c_uint,
            ),
        );
    } else {
        inc_nr_invalid_lps_in_blk(blki);
        inc_nr_invalid_lps(pgi);
        set_pg_status(pgi, lp_ofs, PAGE_STATUS_INVALID);
    }
    set_mapped_lpa(pgi, lp_ofs, lpa);
    if lp_ofs as libc::c_int == NR_LP_IN_PP as libc::c_int - 1 as libc::c_int {
        inc_nr_valid_pgs(blki);
        dec_nr_free_pgs(blki);
    }
    return 0 as libc::c_int;
}
