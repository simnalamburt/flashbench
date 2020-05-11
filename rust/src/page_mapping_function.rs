use crate::constants::*;
use crate::header::*;
use crate::structs::*;

extern "C" {
    fn get_mapped_ppa(ftl: *mut page_mapping_context_t, lpa: u32) -> u32;
    fn set_mapped_ppa(ftl: *mut page_mapping_context_t, lpa: u32, ppa: u32);
    fn get_abm(ftl: *mut page_mapping_context_t) -> *mut fb_act_blk_mngr_t;
    fn set_mapped_lpa(pgi: *mut flash_page, ofs: u8, lpa: u32);
    fn set_pg_status(pgi: *mut flash_page, ofs: u8, status: fb_pg_status_t);
    fn inc_nr_invalid_lps(pgi: *mut flash_page) -> u32;
    fn get_nr_free_pgs(blki: *mut flash_block) -> u32;
    fn inc_nr_valid_pgs(blki: *mut flash_block) -> u32;
    fn inc_nr_invalid_pgs(blki: *mut flash_block) -> u32;
    fn dec_nr_valid_pgs(blki: *mut flash_block) -> u32;
    fn dec_nr_free_pgs(blki: *mut flash_block) -> u32;
    fn inc_nr_valid_lps_in_blk(blk: *mut flash_block) -> u32;
    fn inc_nr_invalid_lps_in_blk(blk: *mut flash_block) -> u32;
    fn dec_nr_valid_lps_in_blk(blk: *mut flash_block) -> u32;
    fn set_act_blk_flag(blki: *mut flash_block, flag: i32);
    fn set_used_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    fn reset_used_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    fn set_dirt_blk(ssdi: *mut ssd_info, bi: *mut flash_block);
    fn get_block_info(
        ptr_ssd_info: *mut ssd_info,
        bus: u32,
        chip: u32,
        block: u32,
    ) -> *mut flash_block;
    fn get_page_info(
        ptr_ssd_info: *mut ssd_info,
        bus: u32,
        chip: u32,
        block: u32,
        page: u32,
    ) -> *mut flash_page;
    fn convert_to_physical_address(bus: u32, chip: u32, block: u32, page: u32) -> u32;
    fn convert_to_ssd_layout(
        logical_page_address: u32,
        ptr_bus: *mut u32,
        ptr_chip: *mut u32,
        ptr_block: *mut u32,
        ptr_page: *mut u32,
    );
}
#[no_mangle]
pub unsafe extern "C" fn get_prev_bus_chip(fb: *mut fb_context_t, bus: *mut u8, chip: *mut u8) {
    let ftl = get_ftl(fb) as *mut page_mapping_context_t;
    let abm = get_abm(ftl);
    *bus = (*abm).mru_bus as u8;
    *chip = (*abm).mru_chip as u8;
}
#[no_mangle]
pub unsafe extern "C" fn alloc_new_page(
    fb: *mut fb_context_t,
    bus: u8,
    chip: u8,
    blk: *mut u32,
    pg: *mut u32,
) -> i32 {
    let ssdi = get_ssd_inf(fb);
    let mut blki: *mut flash_block;
    blki = get_curr_active_block(fb, bus as u32, chip as u32);
    if blki.is_null() {
        blki = get_free_block(ssdi, bus as u32, chip as u32);
        if blki.is_null() {
            return -(1 as i32);
        } else {
            reset_free_blk(ssdi, blki);
            set_curr_active_block(fb, bus as u32, chip as u32, blki);
        }
    }
    *blk = (*blki).no_block;
    *pg = (NUM_PAGES_PER_BLOCK as i32 as u32).wrapping_sub(get_nr_free_pgs(blki));
    *blk = (*blki).no_block;
    *pg = (NUM_PAGES_PER_BLOCK as i32 as u32).wrapping_sub(get_nr_free_pgs(blki));
    return 0 as i32;
}
#[no_mangle]
pub unsafe extern "C" fn map_logical_to_physical(
    ptr_fb_context: *mut fb_context_t,
    logical_page_address: *mut u32,
    bus: u32,
    chip: u32,
    block: u32,
    page: u32,
) -> i32 {
    let mut lp_loop: u8;
    let mut ret = -(1 as i32);
    lp_loop = 0 as i32 as u8;
    while (lp_loop as i32) < NR_LP_IN_PP as i32 {
        ret = __map_logical_to_physical(
            ptr_fb_context,
            *logical_page_address.offset(lp_loop as isize),
            bus,
            chip,
            block,
            page,
            lp_loop,
        );
        if ret == -(1 as i32) {
            break;
        }
        lp_loop = lp_loop.wrapping_add(1)
    }
    return ret;
}
#[no_mangle]
pub unsafe extern "C" fn update_act_blk(fb: *mut fb_context_t, bus: u8, chip: u8) {
    let mut blki = get_curr_active_block(fb, bus as u32, chip as u32);
    if get_nr_free_pgs(blki) == 0 as i32 as u32 {
        let ssdi = get_ssd_inf(fb);
        set_act_blk_flag(blki, false as i32);
        set_used_blk(ssdi, blki);
        blki = get_free_block(ssdi, bus as u32, chip as u32);
        if !blki.is_null() {
            reset_free_blk(ssdi, blki);
        }
        set_curr_active_block(fb, bus as u32, chip as u32, blki);
    };
}
#[no_mangle]
pub unsafe extern "C" fn invalidate_lpg(fb: *mut fb_context_t, lpa: u32) -> u32 {
    let ftl = get_ftl(fb) as *mut page_mapping_context_t;
    let ssdi = get_ssd_inf(fb);
    let ppa = get_mapped_ppa(ftl, lpa);
    if ppa != PAGE_UNMAPPED as i32 as u32 {
        let mut bus = 0u32;
        let mut chip = 0u32;
        let mut blk = 0u32;
        let mut pg = 0u32;
        let blki: *mut flash_block;
        let pgi: *mut flash_page;
        convert_to_ssd_layout(ppa, &mut bus, &mut chip, &mut blk, &mut pg);
        blki = get_block_info(ssdi, bus, chip, blk);
        pgi = get_page_info(ssdi, bus, chip, blk, pg >> LP_PAGE_SHIFT as i32);
        set_pg_status(
            pgi,
            (pg & LP_PAGE_MASK as i32 as u32) as u8,
            PAGE_STATUS_INVALID,
        );
        inc_nr_invalid_lps_in_blk(blki);
        dec_nr_valid_lps_in_blk(blki);
        if inc_nr_invalid_lps(pgi) == NR_LP_IN_PP as i32 as u32 {
            dec_nr_valid_pgs(blki);
            if inc_nr_invalid_pgs(blki) == NUM_PAGES_PER_BLOCK as i32 as u32 {
                reset_used_blk(ssdi, blki);
                if get_curr_gc_block(fb, bus, chip).is_null() {
                    set_curr_gc_block(fb, bus, chip, blki);
                } else {
                    set_dirt_blk(ssdi, blki);
                }
            }
        }
        set_mapped_ppa(ftl, lpa, PAGE_UNMAPPED as i32 as u32);
    }
    return ppa;
}
unsafe extern "C" fn __map_logical_to_physical(
    fb: *mut fb_context_t,
    lpa: u32,
    bus: u32,
    chip: u32,
    blk: u32,
    pg: u32,
    lp_ofs: u8,
) -> i32 {
    let ftl = get_ftl(fb) as *mut page_mapping_context_t;
    let ssdi = get_ssd_inf(fb);
    let blki = get_block_info(ssdi, bus, chip, blk);
    let pgi = get_page_info(ssdi, bus, chip, blk, pg);
    if lpa != PAGE_UNMAPPED as i32 as u32 {
        invalidate_lpg(fb, lpa);
        inc_nr_valid_lps_in_blk(blki);
        set_pg_status(pgi, lp_ofs, PAGE_STATUS_VALID);
        set_mapped_ppa(
            ftl,
            lpa,
            convert_to_physical_address(bus, chip, blk, pg << LP_PAGE_SHIFT as i32 | lp_ofs as u32),
        );
    } else {
        inc_nr_invalid_lps_in_blk(blki);
        inc_nr_invalid_lps(pgi);
        set_pg_status(pgi, lp_ofs, PAGE_STATUS_INVALID);
    }
    set_mapped_lpa(pgi, lp_ofs, lpa);
    if lp_ofs as i32 == NR_LP_IN_PP as i32 - 1 as i32 {
        inc_nr_valid_pgs(blki);
        dec_nr_free_pgs(blki);
    }
    return 0 as i32;
}
