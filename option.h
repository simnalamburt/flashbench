#pragma once
#define TRUE 1
#define FALSE 0

// Parameterf for DEL manager
#define NR_MAX_LPAS_DISCARD 2048
#define NUM_WTODS NR_MAX_LPAS_DISCARD
#define NUM_BTODS NR_MAX_LPAS_DISCARD
#define NR_MAX_LPGS_COPY (NR_MAX_LPAS_DISCARD * 3)

// Parameters for the write buffer
#define WRITE_BUFFER_ENABLE TRUE
#define NUM_PAGES_IN_WRITE_BUFFER (NR_LP_IN_PP * NUM_CHIPS)

// Hardware configuration

#define NUM_BUSES 1
#define NUM_CHIPS_PER_BUS 2
#define NUM_BLOCKS_PER_CHIP 171
#define NUM_PAGES_PER_BLOCK \
  (16 * 4 * 3)  // v-layer * h-layer * multi-level degree
#define PHYSICAL_PAGE_SIZE (LOGICAL_PAGE_SIZE * NR_LP_IN_PP)

#define NUM_CHIPS (NUM_BUSES * NUM_CHIPS_PER_BUS)
#define NUM_BLOCKS (NUM_CHIPS * NUM_BLOCKS_PER_CHIP)
#define NUM_PAGES (NUM_BLOCKS * NUM_PAGES_PER_BLOCK)
#define NUM_LOG_PAGES (NUM_PAGES * NR_LP_IN_PP * CFACTOR_PERCENT / 100)
#define CFACTOR_PERCENT 90
#define NR_LP_IN_PP 4
#define LP_PAGE_MASK 0x3
#define LP_PAGE_SHIFT 2
#define LOGICAL_PAGE_SIZE 4096

#define SECTOR_SIZE 512

#define DEV_NAME "fbSSD"

#define TREAD 80
#define TPROG 800
#define TBERS 5000
#define TPLOCK 100
#define TBLOCK 100

#define NUM_MAX_ENTRIES_OPR_QUEUE 4

#define LOG_TIMING FALSE

#define BGC_TH_INTV 5000
#define BGC_TH_WB_UTIL 5
#define BGC_TH_NR_BLKS 14
