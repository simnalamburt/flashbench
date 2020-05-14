//
// libflashbench.h
//

// vdecice
#[repr(C)]
#[allow(non_camel_case_types)]
pub enum fb_dev_op_t {
    OP_READ = 0,
    OP_PROG = 1,
    OP_PLOCK = 2,
    OP_BLOCK = 3,
    OP_BERS = 4,
}
pub use fb_dev_op_t::*;

// Hardware configuration
pub const NUM_BUSES: u32 = 1;

pub const NUM_CHIPS_PER_BUS: u32 = 2;
pub const NUM_BLOCKS_PER_CHIP: u32 = 171;
/// v-layer * h-layer * multi-level degree
pub const NUM_PAGES_PER_BLOCK: u32 = 16 * 4 * 3;

//
// option.h
//

// Hardware configuration
pub const CFACTOR_PERCENT: u32 = 90;
pub const NR_LP_IN_PP: u32 = 4;
pub const LP_PAGE_MASK: u32 = 3;
pub const LP_PAGE_SHIFT: u32 = 2;

pub const NUM_CHIPS: u32 = NUM_BUSES * NUM_CHIPS_PER_BUS;
pub const NUM_BLOCKS: u32 = NUM_CHIPS * NUM_BLOCKS_PER_CHIP;
pub const NUM_PAGES: u32 = NUM_BLOCKS * NUM_PAGES_PER_BLOCK;
pub const NUM_LOG_PAGES: u32 = NUM_PAGES * NR_LP_IN_PP * CFACTOR_PERCENT / 100;

pub const LOGICAL_PAGE_SIZE: u32 = 4096;
pub const PHYSICAL_PAGE_SIZE: u32 = LOGICAL_PAGE_SIZE * NR_LP_IN_PP;

pub const NUM_MAX_ENTRIES_OPR_QUEUE: u32 = 4;

pub const BGC_TH_NR_BLKS: u32 = 14;

//
// Other macros
//
pub const PAGE_UNMAPPED: i32 = -1;

#[repr(C)]
#[allow(non_camel_case_types)]
pub enum fb_pg_status_t {
    // PAGE_STATUS_FREE = 0,
    PAGE_STATUS_VALID = 1,
    PAGE_STATUS_INVALID = 2,
    // PAGE_STATUS_DEL = 3,
}
pub use fb_pg_status_t::*;
