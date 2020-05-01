extern {
    pub type flash_page;
    pub type fb_context_t;
    pub type fb_bio_t;
    pub type page_mapping_context_t;
    pub type ssd_info;
    pub type fb_bus_controller_t;
    pub type flash_chip;
}

#[repr(C)]
pub struct flash_block {
    pub no_block: u32,
    pub no_chip: u32,
    pub no_bus: u32,
    pub nr_valid_pages: u32,
    pub nr_invalid_pages: u32,
    pub nr_free_pages: u32,
    pub nr_valid_log_pages: u32,
    pub nr_invalid_log_pages: u32,
    pub nr_erase_cnt: u32,
    pub is_bad_block: i32,
    pub last_modified_time: u32,
    pub is_reserved_block: i32,
    pub is_active_block: i32,
    pub list_pages: *mut flash_page,
    pub prev: *mut flash_block,
    pub next: *mut flash_block,
    pub del_flag: i32,
}

#[repr(C)]
pub struct fb_act_blk_mngr_t {
    pub act_blks: *mut *mut flash_block,
    pub mru_bus: u32,
    pub mru_chip: u32,
}

#[repr(C)]
pub struct fb_gc_mngr_t {
    pub gc_blks: *mut *mut flash_block,
    pub vic_blks: *mut *mut flash_block,
    pub first_valid_pg: *mut u32,
    pub lpas_to_copy: *mut u32,
    pub data_to_copy: *mut u8,
    pub nr_pgs_to_copy: u32,
}

#[repr(C)]
pub struct vdevice_t {
    pub device_capacity: u64,
    pub logical_capacity: u64,
    pub ptr_vdisk: [*mut u8; 1],
    pub buses: [vdevice_bus_t; 1],
    pub ptr_bus_controller: *mut *mut fb_bus_controller_t,
}

#[repr(C)]
pub struct vdevice_bus_t {
    pub chips: [vdevice_chip_t; 2],
}

#[repr(C)]
pub struct vdevice_chip_t {
    pub blocks: [vdevice_block_t; 171],
}

#[repr(C)]
pub struct vdevice_block_t {
    pub pages: [vdevice_page_t; 192],
}

#[repr(C)]
pub struct vdevice_page_t {
    pub ptr_data: *mut u8,
}
