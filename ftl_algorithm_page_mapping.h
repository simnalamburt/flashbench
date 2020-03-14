#pragma once
#include <linux/types.h>

enum {
  PAGE_UNMAPPED = -1,
};

struct flash_block;
struct fb_del_mngr_t;
struct fb_context_t;
struct fb_bio_t;
struct page_mapping_context_t;

struct page_mapping_table_t {
  u32 nr_entries;
  u32 *mappings;
};

struct fb_act_blk_mngr_t {
  struct flash_block **act_blks;

  u32 mru_bus;
  u32 mru_chip;
};

struct fb_gc_mngr_t {
  struct flash_block **gc_blks;

  struct flash_block **vic_blks;
  u32 *first_valid_pg;

  u32 *lpas_to_copy;
  u8 *data_to_copy;

  u32 nr_pgs_to_copy;
};

void *create_pg_ftl(struct fb_context_t *fb);
void destroy_pg_ftl(struct page_mapping_context_t *ftl);
struct fb_gc_mngr_t *get_gcm(struct page_mapping_context_t *ftl);
struct fb_act_blk_mngr_t *get_abm(struct page_mapping_context_t *ftl);
void print_blk_mgmt(struct fb_context_t *fb);
int fb_del_invalid_data(struct fb_context_t *fb, struct fb_bio_t *fb_bio);
