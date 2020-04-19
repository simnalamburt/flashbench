//
// libflashbench.h
//

// vdecice
#[allow(non_camel_case_types)] pub type fb_dev_op_t = u32;
pub const OP_READ: fb_dev_op_t = 0;
pub const OP_PROG: fb_dev_op_t = 1;
pub const OP_PLOCK: fb_dev_op_t = 2;
pub const OP_BLOCK: fb_dev_op_t = 3;
pub const OP_BERS: fb_dev_op_t = 4;

// Hardware configuration
pub const NUM_BUSES: u32 = 1;
pub const NUM_CHIPS_PER_BUS: u32 = 2;
pub const NUM_BLOCKS_PER_CHIP: u32 = 171;
/// v-layer * h-layer * multi-level degree
pub const NUM_PAGES_PER_BLOCK: u32 = 16 * 4 * 3;

//
// option.h
//

// Parameter for DEL manager
pub const NR_MAX_LPAS_DISCARD: u32 = 2048;
pub const NUM_WTODS: u32 = 2048;
pub const NUM_BTODS: u32 = 2048;
pub const NR_MAX_LPGS_COPY: u32 = 6144;

// Hardware configuration
pub const CFACTOR_PERCENT: u32 = 90;
pub const NR_LP_IN_PP: u32 = 4;
pub const LP_PAGE_MASK: u32 = 3;
pub const LP_PAGE_SHIFT: u32 = 2;

pub const NUM_CHIPS: u32 = NUM_BUSES * NUM_CHIPS_PER_BUS;
pub const NUM_BLOCKS: u32 = NUM_CHIPS * NUM_BLOCKS_PER_CHIP;
pub const NUM_PAGES: u32 = NUM_BLOCKS * NUM_PAGES_PER_BLOCK;
pub const NUM_PAGES_IN_WRITE_BUFFER: u32 = NR_LP_IN_PP * NUM_CHIPS;
pub const NUM_LOG_PAGES: u32 = NUM_PAGES * NR_LP_IN_PP * CFACTOR_PERCENT / 100;

pub const LOGICAL_PAGE_SIZE: u32 = 4096;
pub const PHYSICAL_PAGE_SIZE: u32 = LOGICAL_PAGE_SIZE * NR_LP_IN_PP;
pub const SECTOR_SIZE: u32 = 512;

pub const TREAD: u32 = 80;
pub const TPROG: u32 = 800;
pub const TBERS: u32 = 5000;
pub const TPLOCK: u32 = 100;
pub const TBLOCK: u32 = 100;

pub const NUM_MAX_ENTRIES_OPR_QUEUE: u32 = 4;

pub const BGC_TH_INTV: u32 = 5000;
pub const BGC_TH_WB_UTIL: u32 = 5;
pub const BGC_TH_NR_BLKS: u32 = 14;

//
// Other macros
//
pub const PAGE_UNMAPPED: i32 = -1;

#[repr(C)]
pub enum fb_pg_status_t {
    // PAGE_STATUS_FREE = 0,
    PAGE_STATUS_VALID = 1,
    PAGE_STATUS_INVALID = 2,
    // PAGE_STATUS_DEL = 3,
}
pub use fb_pg_status_t::*;
