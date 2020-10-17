use crate::structs::*;

extern "C" {
    pub fn get_curr_active_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
    ) -> *mut flash_block;

    pub fn get_curr_gc_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
    ) -> *mut flash_block;

    pub fn get_free_block(ssdi: *mut ssd_info, bus: u32, chip: u32) -> *mut flash_block;

    pub fn get_ftl(fb: *mut fb_context_t) -> *mut page_mapping_context_t;

    pub fn get_ssd_inf(fb: *mut fb_context_t) -> *mut ssd_info;

    pub fn reset_free_blk(ssdi: *mut ssd_info, bi: *mut flash_block);

    pub fn set_curr_active_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
        ptr_new_block: *mut flash_block,
    );

    pub fn set_curr_gc_block(
        ptr_fb_context: *mut fb_context_t,
        bus: u32,
        chip: u32,
        ptr_new_block: *mut flash_block,
    );
}
