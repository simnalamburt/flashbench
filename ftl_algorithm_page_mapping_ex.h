#pragma once
#include <linux/completion.h>
#include <linux/types.h>

struct page_mapping_table_t;
struct fb_act_blk_mngr_t;
struct fb_gc_mngr_t;
struct fb_del_mngr_t;

struct page_mapping_context_t {
  struct completion mapping_context_lock;

  struct page_mapping_table_t *ptr_mapping_table;

  struct fb_act_blk_mngr_t *abm;

  struct fb_gc_mngr_t *gcm;

  struct fb_del_mngr_t *delm;

  u32 *lpas_to_discard;
};
