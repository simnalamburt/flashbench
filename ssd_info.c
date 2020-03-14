#include <linux/slab.h>

#include "ftl_algorithm_page_mapping.h"
#include "option.h"
#include "ssd_info.h"
#include "uthash/utlist.h"

#define IDLE 0

static void set_nr_invalid_lps(struct flash_page *pgi, u32 value);
static void set_del_flag_pg(struct flash_page *pgi, int flag);
static u32 get_bus_idx(struct flash_block *blki);
static u32 get_chip_idx(struct flash_block *blki);

static void set_nr_valid_pgs(struct flash_block *blki, u32 value);
static void set_nr_invalid_pgs(struct flash_block *blki, u32 value);
static void set_nr_free_pgs(struct flash_block *blki, u32 value);

static void set_nr_valid_lps_in_blk(struct flash_block *blki, u32 value);
static void set_nr_invalid_lps_in_blk(struct flash_block *blki, u32 value);

static void set_del_flag_blk(struct flash_block *blki, int flag);

struct flash_page {
  enum fb_pg_status_t page_status[NR_LP_IN_PP];
  u32 no_logical_page_addr[NR_LP_IN_PP];
  u32 nr_invalid_log_pages;

  int del_flag;
};

struct flash_chip {
  struct completion chip_status_lock;

  int chip_status;
  // the number of free blocks in a chip
  u32 nr_free_blocks;
  u32 nr_used_blks;
  u32 nr_dirt_blks;

  struct flash_block *free_blks;
  struct flash_block *used_blks;
  struct flash_block *dirt_blks;

  // the number of dirty blocks in a chip
  u32 nr_dirty_blocks;

  // block array
  struct flash_block *list_blocks;
};

struct flash_bus {
  // chip array
  struct flash_chip *list_chips;
};

struct ssd_info {
  // ssd information
  u32 nr_buses;
  u32 nr_chips_per_bus;
  u32 nr_blocks_per_chip;
  u32 nr_pages_per_block;

  // bus array
  struct flash_bus *list_buses;
};

void set_free_blk(struct ssd_info *ssdi, struct flash_block *bi) {
  struct flash_chip *ci =
      get_chip_info(ssdi, get_bus_idx(bi), get_chip_idx(bi));

  DL_APPEND(ci->free_blks, bi);
  ci->nr_free_blocks++;
}

void reset_free_blk(struct ssd_info *ssdi, struct flash_block *bi) {
  struct flash_chip *ci =
      get_chip_info(ssdi, get_bus_idx(bi), get_chip_idx(bi));

  DL_DELETE(ci->free_blks, bi);
  ci->nr_free_blocks--;
}

struct flash_block *get_free_block(struct ssd_info *ssdi, u32 bus, u32 chip) {
  struct flash_chip *ci = get_chip_info(ssdi, bus, chip);

  return ci->free_blks;
}

u32 get_nr_free_blks_in_chip(struct flash_chip *ci) {
  return ci->nr_free_blocks;
}

void set_used_blk(struct ssd_info *ssdi, struct flash_block *bi) {
  struct flash_chip *ci =
      get_chip_info(ssdi, get_bus_idx(bi), get_chip_idx(bi));

  DL_APPEND(ci->used_blks, bi);
  ci->nr_used_blks++;
}

void reset_used_blk(struct ssd_info *ssdi, struct flash_block *bi) {
  struct flash_chip *ci =
      get_chip_info(ssdi, get_bus_idx(bi), get_chip_idx(bi));

  DL_DELETE(ci->used_blks, bi);
  ci->nr_used_blks--;
}

struct flash_block *get_used_block(struct ssd_info *ssdi, u32 bus, u32 chip) {
  struct flash_chip *ci = get_chip_info(ssdi, bus, chip);

  return ci->used_blks;
}

u32 get_nr_used_blks_in_chip(struct flash_chip *ci) { return ci->nr_used_blks; }

void set_dirt_blk(struct ssd_info *ssdi, struct flash_block *bi) {
  struct flash_chip *ci =
      get_chip_info(ssdi, get_bus_idx(bi), get_chip_idx(bi));

  DL_APPEND(ci->dirt_blks, bi);
  ci->nr_dirt_blks++;
}

void reset_dirt_blk(struct ssd_info *ssdi, struct flash_block *bi) {
  struct flash_chip *ci =
      get_chip_info(ssdi, get_bus_idx(bi), get_chip_idx(bi));

  DL_DELETE(ci->dirt_blks, bi);
  ci->nr_dirt_blks--;
}

struct flash_block *get_dirt_block(struct ssd_info *ssdi, u32 bus, u32 chip) {
  struct flash_chip *ci = get_chip_info(ssdi, bus, chip);

  return ci->dirt_blks;
}

u32 get_nr_dirt_blks_in_chip(struct flash_chip *ci) { return ci->nr_dirt_blks; }

struct ssd_info *create_ssd_info(void) {
  u32 loop_bus, loop_chip, loop_block, loop_page;

  struct ssd_info *ptr_ssd_info = NULL;

  if ((ptr_ssd_info = (struct ssd_info *)kmalloc(sizeof(struct ssd_info),
                                                 GFP_ATOMIC)) == NULL) {
    printk(KERN_ERR
           "[FlashBench] ssd_info: Allocating ssd information structure "
           "failed.\n");
    goto FAIL_ALLOC_SSD_INFO;
  }

  ptr_ssd_info->nr_buses = NUM_BUSES;
  ptr_ssd_info->nr_chips_per_bus = NUM_CHIPS_PER_BUS;
  ptr_ssd_info->nr_blocks_per_chip = NUM_BLOCKS_PER_CHIP;
  ptr_ssd_info->nr_pages_per_block = NUM_PAGES_PER_BLOCK;

  if ((ptr_ssd_info->list_buses = (struct flash_bus *)kmalloc(
           sizeof(struct flash_bus) * NUM_BUSES, GFP_ATOMIC)) == NULL) {
    printk(KERN_ERR
           "[FlashBench] ssd_info: Allocating bus information structure "
           "failed.\n");
    goto FAIL_ALLOC_BUS_INFO;
  }

  for (loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
    struct flash_bus *ptr_bus = &ptr_ssd_info->list_buses[loop_bus];

    // initialize flash chip
    if ((ptr_bus->list_chips = (struct flash_chip *)kmalloc(
             sizeof(struct flash_chip) * NUM_CHIPS_PER_BUS, GFP_ATOMIC)) ==
        NULL) {
      printk(KERN_ERR
             "[FlashBench] ssd_info: Allocating chip information structure "
             "failed.\n");
      goto FAIL_ALLOC_INFOS;
    }

    for (loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
      struct flash_chip *ptr_chip = &ptr_bus->list_chips[loop_chip];

      init_completion(&ptr_chip->chip_status_lock);
      complete(&ptr_chip->chip_status_lock);

      ptr_chip->chip_status = IDLE;

      ptr_chip->list_blocks = NULL;
      ptr_chip->nr_free_blocks = 0;
      ptr_chip->nr_used_blks = 0;
      ptr_chip->nr_dirt_blks = 0;
      ptr_chip->nr_dirty_blocks = 0;

      ptr_chip->free_blks = NULL;
      ptr_chip->used_blks = NULL;
      ptr_chip->dirt_blks = NULL;

      // initialize blocks
      if ((ptr_chip->list_blocks = (struct flash_block *)kmalloc(
               sizeof(struct flash_block) * NUM_BLOCKS_PER_CHIP, GFP_ATOMIC)) ==
          NULL) {
        printk(KERN_ERR
               "[FlashBench] ssd_info: Allocating block information structure "
               "failed.\n");
        goto FAIL_ALLOC_INFOS;
      }

      for (loop_block = 0; loop_block < NUM_BLOCKS_PER_CHIP; loop_block++) {
        struct flash_block *ptr_block = &ptr_chip->list_blocks[loop_block];

        ptr_block->list_pages = NULL;
        ptr_block->no_block = loop_block;
        ptr_block->no_chip = loop_chip;
        ptr_block->no_bus = loop_bus;

        ptr_block->nr_valid_log_pages = 0;
        ptr_block->nr_invalid_log_pages = 0;
        ptr_block->nr_valid_pages = 0;
        ptr_block->nr_invalid_pages = 0;
        ptr_block->nr_free_pages = NUM_PAGES_PER_BLOCK;

        ptr_block->nr_erase_cnt = 0;
        ptr_block->is_bad_block = FALSE;
        ptr_block->is_reserved_block = FALSE;
        ptr_block->is_active_block = FALSE;
        ptr_block->last_modified_time = 0;

        set_del_flag_blk(ptr_block, FALSE);
        // timestamp is set to 0 if the block is not used after initialization

        if ((ptr_block->list_pages = (struct flash_page *)kmalloc(
                 sizeof(struct flash_page) * NUM_PAGES_PER_BLOCK,
                 GFP_ATOMIC)) == NULL) {
          printk(KERN_ERR
                 "[FlashBench] ssd_info: Allocating page information structure "
                 "failed.\n");
          goto FAIL_ALLOC_INFOS;
        }

        for (loop_page = 0; loop_page < NUM_PAGES_PER_BLOCK; loop_page++) {
          struct flash_page *ptr_page = &ptr_block->list_pages[loop_page];
          u8 lp_loop;

          ptr_page->nr_invalid_log_pages = 0;
          for (lp_loop = 0; lp_loop < NR_LP_IN_PP; lp_loop++) {
            ptr_page->no_logical_page_addr[lp_loop] =
                -1;  // free page (by default)
            ptr_page->page_status[lp_loop] =
                PAGE_STATUS_FREE;  // free page (by default)
          }
          set_del_flag_pg(ptr_page, FALSE);
        }

        set_free_blk(ptr_ssd_info, ptr_block);
      }
    }
  }

  return ptr_ssd_info;

FAIL_ALLOC_INFOS:
  if (ptr_ssd_info->list_buses != NULL) {
    for (loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
      struct flash_bus *ptr_bus = &ptr_ssd_info->list_buses[loop_bus];
      if (ptr_bus->list_chips != NULL) {
        for (loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
          struct flash_chip *ptr_chip = &ptr_bus->list_chips[loop_chip];
          if (ptr_chip->list_blocks != NULL) {
            for (loop_block = 0; loop_block < NUM_BLOCKS_PER_CHIP;
                 loop_block++) {
              struct flash_block *ptr_block =
                  &ptr_chip->list_blocks[loop_block];
              if (ptr_block->list_pages != NULL) {
                kfree(ptr_block->list_pages);
              }
            }

            kfree(ptr_chip->list_blocks);
          }
        }
        kfree(ptr_bus->list_chips);
      }
    }
    kfree(ptr_ssd_info->list_buses);
  }

FAIL_ALLOC_BUS_INFO:
  if (ptr_ssd_info != NULL) {
    kfree(ptr_ssd_info);
  }

FAIL_ALLOC_SSD_INFO:
  return NULL;
}

void destroy_ssd_info(struct ssd_info *ptr_ssd_info) {
  u32 loop_bus, loop_chip, loop_block;

  if (ptr_ssd_info != NULL) {
    if (ptr_ssd_info->list_buses != NULL) {
      for (loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
        struct flash_bus *ptr_bus = &ptr_ssd_info->list_buses[loop_bus];
        if (ptr_bus->list_chips != NULL) {
          for (loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
            struct flash_chip *ptr_chip = &ptr_bus->list_chips[loop_chip];
            if (ptr_chip->list_blocks != NULL) {
              for (loop_block = 0; loop_block < NUM_BLOCKS_PER_CHIP;
                   loop_block++) {
                struct flash_block *ptr_block =
                    &ptr_chip->list_blocks[loop_block];
                if (ptr_block->list_pages != NULL) {
                  kfree(ptr_block->list_pages);
                }
              }

              kfree(ptr_chip->list_blocks);
            }
          }
          kfree(ptr_bus->list_chips);
        }
      }
      kfree(ptr_ssd_info->list_buses);
    }

    kfree(ptr_ssd_info);
  }
}

struct flash_chip *get_chip_info(struct ssd_info *ptr_ssd_info, u32 bus,
                                 u32 chip) {
  return &(ptr_ssd_info->list_buses[bus].list_chips[chip]);
}

struct flash_block *get_block_info(struct ssd_info *ptr_ssd_info, u32 bus,
                                   u32 chip, u32 block) {
  return &(ptr_ssd_info->list_buses[bus].list_chips[chip].list_blocks[block]);
}

struct flash_page *get_page_info(struct ssd_info *ptr_ssd_info, u32 bus,
                                 u32 chip, u32 block, u32 page) {
  return &(ptr_ssd_info->list_buses[bus]
               .list_chips[chip]
               .list_blocks[block]
               .list_pages[page]);
}

u32 get_mapped_lpa(struct flash_page *pgi, u8 ofs) {
  return pgi->no_logical_page_addr[ofs];
}

void set_mapped_lpa(struct flash_page *pgi, u8 ofs, u32 lpa) {
  pgi->no_logical_page_addr[ofs] = lpa;
}

enum fb_pg_status_t get_pg_status(struct flash_page *pgi, u8 ofs) {
  return pgi->page_status[ofs];
}

void set_pg_status(struct flash_page *pgi, u8 ofs, enum fb_pg_status_t status) {
  pgi->page_status[ofs] = status;
}

u32 get_nr_invalid_lps(struct flash_page *pgi) {
  return pgi->nr_invalid_log_pages;
}

static void set_nr_invalid_lps(struct flash_page *pgi, u32 value) {
  pgi->nr_invalid_log_pages = value;
}

u32 inc_nr_invalid_lps(struct flash_page *pgi) {
  return ++(pgi->nr_invalid_log_pages);
}

static void set_del_flag_pg(struct flash_page *pgi, int flag) {
  pgi->del_flag = flag;
}

void init_blk_inf(struct flash_block *blki) {
  u8 lp;
  u32 pg;
  struct flash_page *pgi;

  set_act_blk_flag(blki, FALSE);
  set_rsv_blk_flag(blki, FALSE);

  set_nr_free_pgs(blki, NUM_PAGES_PER_BLOCK);
  set_nr_valid_pgs(blki, 0);
  set_nr_invalid_pgs(blki, 0);
  set_nr_valid_lps_in_blk(blki, 0);
  set_nr_invalid_lps_in_blk(blki, 0);

  set_del_flag_blk(blki, FALSE);

  for (pg = 0; pg < NUM_PAGES_PER_BLOCK; pg++) {
    pgi = get_pgi_from_blki(blki, pg);

    set_nr_invalid_lps(pgi, 0);

    set_del_flag_pg(pgi, FALSE);

    for (lp = 0; lp < NR_LP_IN_PP; lp++) {
      set_mapped_lpa(pgi, lp, PAGE_UNMAPPED);
      set_pg_status(pgi, lp, PAGE_STATUS_FREE);
    }
  }
}

static u32 get_bus_idx(struct flash_block *blki) { return blki->no_bus; }

static u32 get_chip_idx(struct flash_block *blki) { return blki->no_chip; }

u32 get_blk_idx(struct flash_block *blki) { return blki->no_block; }

struct flash_page *get_pgi_from_blki(struct flash_block *blki, u32 pg) {
  return (blki->list_pages + pg);
}

u32 get_nr_free_pgs(struct flash_block *blki) { return blki->nr_free_pages; }

static void set_nr_valid_pgs(struct flash_block *blki, u32 value) {
  blki->nr_valid_pages = value;
}

static void set_nr_invalid_pgs(struct flash_block *blki, u32 value) {
  blki->nr_invalid_pages = value;
}

static void set_nr_free_pgs(struct flash_block *blki, u32 value) {
  blki->nr_free_pages = value;
}

u32 inc_nr_valid_pgs(struct flash_block *blki) {
  return ++(blki->nr_valid_pages);
}

u32 inc_nr_invalid_pgs(struct flash_block *blki) {
  return ++(blki->nr_invalid_pages);
}

u32 dec_nr_valid_pgs(struct flash_block *blki) {
  return --(blki->nr_valid_pages);
}

u32 dec_nr_free_pgs(struct flash_block *blki) {
  return --(blki->nr_free_pages);
}

u32 get_nr_valid_lps_in_blk(struct flash_block *blki) {
  return blki->nr_valid_log_pages;
}

u32 get_nr_invalid_lps_in_blk(struct flash_block *blki) {
  return blki->nr_invalid_log_pages;
}

static void set_nr_valid_lps_in_blk(struct flash_block *blki, u32 value) {
  blki->nr_valid_log_pages = value;
}

static void set_nr_invalid_lps_in_blk(struct flash_block *blki, u32 value) {
  blki->nr_invalid_log_pages = value;
}

u32 inc_nr_valid_lps_in_blk(struct flash_block *blki) {
  return ++(blki->nr_valid_log_pages);
}

u32 inc_nr_invalid_lps_in_blk(struct flash_block *blki) {
  return ++(blki->nr_invalid_log_pages);
}

u32 dec_nr_valid_lps_in_blk(struct flash_block *blki) {
  return --(blki->nr_valid_log_pages);
}

u32 inc_bers_cnt(struct flash_block *blki) { return ++(blki->nr_erase_cnt); }

void set_rsv_blk_flag(struct flash_block *blki, int flag) {
  blki->is_reserved_block = flag;
}

void set_act_blk_flag(struct flash_block *blki, int flag) {
  blki->is_active_block = flag;
}

static void set_del_flag_blk(struct flash_block *blki, int flag) {
  blki->del_flag = flag;
}
