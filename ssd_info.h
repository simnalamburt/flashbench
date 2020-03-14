#pragma once
#include <linux/types.h>

struct flash_page;
struct flash_chip;
struct flash_bus;
struct ssd_info;

enum fb_pg_status_t {
  PAGE_STATUS_FREE,
  PAGE_STATUS_VALID,
  PAGE_STATUS_INVALID,
  PAGE_STATUS_DEL
};

struct flash_block {
  // block id
  u32 no_block;
  u32 no_chip;
  u32 no_bus;

  // block status
  u32 nr_valid_pages;
  u32 nr_invalid_pages;
  u32 nr_free_pages;

  u32 nr_valid_log_pages;
  u32 nr_invalid_log_pages;

  // the number of erasure operations
  u32 nr_erase_cnt;

  // for bad block management
  int is_bad_block;

  // for garbage collection
  u32 last_modified_time;

  // is reserved block for gc?
  int is_reserved_block;

  // is active block?
  int is_active_block;

  // for keeping the oob data (NOTE: it must be removed in real implementation)
  struct flash_page *list_pages;

  struct flash_block *prev, *next;

  int del_flag;
};

// page info interface
u32 get_mapped_lpa(struct flash_page *pgi, u8 ofs);
void set_mapped_lpa(struct flash_page *pgi, u8 ofs, u32 lpa);
enum fb_pg_status_t get_pg_status(struct flash_page *pgi, u8 ofs);
void set_pg_status(struct flash_page *pgi, u8 ofs, enum fb_pg_status_t status);
u32 get_nr_invalid_lps(struct flash_page *pgi);
u32 inc_nr_invalid_lps(struct flash_page *pgi);

// block info interface
void init_blk_inf(struct flash_block *blki);
u32 get_blk_idx(struct flash_block *blki);
struct flash_page *get_pgi_from_blki(struct flash_block *blki, u32 pg);

u32 get_nr_free_pgs(struct flash_block *blki);
u32 inc_nr_valid_pgs(struct flash_block *blki);
u32 inc_nr_invalid_pgs(struct flash_block *blki);
u32 dec_nr_valid_pgs(struct flash_block *blki);
u32 dec_nr_free_pgs(struct flash_block *blki);

u32 get_nr_valid_lps_in_blk(struct flash_block *blki);
u32 get_nr_invalid_lps_in_blk(struct flash_block *blki);
u32 inc_nr_valid_lps_in_blk(struct flash_block *blk);
u32 inc_nr_invalid_lps_in_blk(struct flash_block *blk);
u32 dec_nr_valid_lps_in_blk(struct flash_block *blk);

u32 inc_bers_cnt(struct flash_block *blki);

void set_rsv_blk_flag(struct flash_block *blki, int flag);
void set_act_blk_flag(struct flash_block *blki, int flag);

// chip info interface
void set_free_blk(struct ssd_info *ssdi, struct flash_block *bi);
void reset_free_blk(struct ssd_info *ssdi, struct flash_block *bi);
struct flash_block *get_free_block(struct ssd_info *ssdi, u32 bus, u32 chip);
u32 get_nr_free_blks_in_chip(struct flash_chip *ci);
void set_used_blk(struct ssd_info *ssdi, struct flash_block *bi);
void reset_used_blk(struct ssd_info *ssdi, struct flash_block *bi);
struct flash_block *get_used_block(struct ssd_info *ssdi, u32 bus, u32 chip);
u32 get_nr_used_blks_in_chip(struct flash_chip *ci);
void set_dirt_blk(struct ssd_info *ssdi, struct flash_block *bi);
void reset_dirt_blk(struct ssd_info *ssdi, struct flash_block *bi);
struct flash_block *get_dirt_block(struct ssd_info *ssdi, u32 bus, u32 chip);
u32 get_nr_dirt_blks_in_chip(struct flash_chip *ci);

// create the ftl information structure
struct ssd_info *create_ssd_info(void);

// destory the ftl information structure
void destroy_ssd_info(struct ssd_info *ptr_ssd_info);

struct flash_chip *get_chip_info(struct ssd_info *ptr_ssd_info, u32 bus,
                                 u32 chip);

struct flash_block *get_block_info(struct ssd_info *ptr_ssd_info, u32 bus,
                                   u32 chip, u32 block);

struct flash_page *get_page_info(struct ssd_info *ptr_ssd_info, u32 bus,
                                 u32 chip, u32 block, u32 page);
