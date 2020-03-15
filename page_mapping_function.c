#include "ftl_algorithm_page_mapping.h"
#include "ftl_algorithm_page_mapping_ex.h"
#include "rust/libflashbench.h"
#include "main_ex.h"
#include "ssd_info.h"

u32 get_mapped_physical_address(struct fb_context_t *ptr_fb_context,
                                u32 logical_page_address, u32 *ptr_bus,
                                u32 *ptr_chip, u32 *ptr_block, u32 *ptr_page) {
  struct page_mapping_context_t *ptr_mapping_context =
      (struct page_mapping_context_t *)ptr_fb_context->ptr_mapping_context;
  u32 physical_page_address = (u32)PAGE_UNMAPPED;

  if (is_valid_address_range(logical_page_address) == false) {
    printk(KERN_ERR
           "flashbench: fb_page_mapping: Invalid logical page range (%d)\n",
           logical_page_address);
    goto FINISH;
  }

  physical_page_address =
      ptr_mapping_context->ptr_mapping_table->mappings[logical_page_address];
  if (physical_page_address != (u32)PAGE_UNMAPPED) {
    convert_to_ssd_layout(physical_page_address, ptr_bus, ptr_chip, ptr_block,
                          ptr_page);
  }

FINISH:
  return physical_page_address;
}

void set_prev_bus_chip(struct fb_context_t *ptr_fb_context, u8 bus, u8 chip) {
  struct page_mapping_context_t *ptr_mapping_context =
      (struct page_mapping_context_t *)ptr_fb_context->ptr_mapping_context;
  struct fb_act_blk_mngr_t *abm = ptr_mapping_context->abm;

  abm->mru_bus = bus;
  abm->mru_chip = chip;
}

void get_next_bus_chip(struct fb_context_t *ptr_fb_context, u8 *ptr_bus,
                       u8 *ptr_chip) {
  struct page_mapping_context_t *ptr_mapping_context =
      (struct page_mapping_context_t *)ptr_fb_context->ptr_mapping_context;
  struct fb_act_blk_mngr_t *abm = ptr_mapping_context->abm;

  if (abm->mru_bus == NUM_BUSES - 1) {
    *ptr_bus = 0;
    *ptr_chip =
        (abm->mru_chip == NUM_CHIPS_PER_BUS - 1) ? 0 : abm->mru_chip + 1;
  } else {
    *ptr_bus = abm->mru_bus + 1;
    *ptr_chip = abm->mru_chip;
  }
}

u32 get_mapped_ppa(struct page_mapping_context_t *ftl, u32 lpa) {
  return ftl->ptr_mapping_table->mappings[lpa];
}

void set_mapped_ppa(struct page_mapping_context_t *ftl, u32 lpa, u32 ppa) {
  ftl->ptr_mapping_table->mappings[lpa] = ppa;
}

struct flash_block *get_curr_gc_block(struct fb_context_t *fb, u32 bus,
                                      u32 chip) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);

  return ftl->gcm->gc_blks[bus * NUM_CHIPS_PER_BUS + chip];
}

void set_curr_gc_block(struct fb_context_t *fb, u32 bus, u32 chip,
                       struct flash_block *blki) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);

  *(ftl->gcm->gc_blks + (bus * NUM_CHIPS_PER_BUS + chip)) = blki;

  if (blki != NULL) set_rsv_blk_flag(blki, true);
}

struct flash_block *get_curr_active_block(struct fb_context_t *fb, u32 bus,
                                          u32 chip) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);

  return ftl->abm->act_blks[bus * NUM_CHIPS_PER_BUS + chip];
}

void set_curr_active_block(struct fb_context_t *fb, u32 bus, u32 chip,
                           struct flash_block *blki) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);

  *(ftl->abm->act_blks + (bus * NUM_CHIPS_PER_BUS + chip)) = blki;

  if (blki != NULL) set_act_blk_flag(blki, true);
}
