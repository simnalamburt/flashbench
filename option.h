#pragma once
#include "rust/libflashbench.h"

//
// Hardware configuration
//
enum {
  CFACTOR_PERCENT = 90,
  NR_LP_IN_PP = 4,
  LP_PAGE_MASK = 0x3,
  LP_PAGE_SHIFT = 2,

  NUM_CHIPS = NUM_BUSES * NUM_CHIPS_PER_BUS,
  NUM_PAGES_IN_WRITE_BUFFER = NR_LP_IN_PP * NUM_CHIPS,

  LOGICAL_PAGE_SIZE = 4096,
  PHYSICAL_PAGE_SIZE = LOGICAL_PAGE_SIZE * NR_LP_IN_PP,
  SECTOR_SIZE = 512,

  BGC_TH_INTV = 5000,
};
