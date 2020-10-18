#include <linux/blk_types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "rust/libflashbench.h"

#include "ftl_algorithm_page_mapping.h"
#include "gc_page_mapping.h"
#include "main.h"
#include "option.h"
#include "page_mapping_function.h"
#include "ssd_info.h"
#include "util.h"
#include "write_buffer.h"

// Parameter for DEL manager
enum {
  NR_MAX_LPAS_DISCARD = 2048,
  NUM_WTODS = NR_MAX_LPAS_DISCARD,
  NUM_BTODS = NR_MAX_LPAS_DISCARD,
  NR_MAX_LPGS_COPY = NR_MAX_LPAS_DISCARD * 3,
};

// block to del
struct fb_btod_t {
  struct flash_block *blki;

  UT_hash_handle hh;
};

// wordline to del
struct fb_wtod_t {
  u32 wl_idx;
  u32 bus;
  u32 chip;

  UT_hash_handle hh;
};

struct fb_del_mngr_t {
  // list of pages (blocks) to lock - physical address base
  // list of pages to copy - logical address base
  // data buffers for pages to copy
  u32 *ppas;

  u32 nr_btod;
  struct fb_btod_t *btod;
  struct fb_btod_t *hash_btod;

  u32 nr_wtod;
  struct fb_wtod_t *wtod;
  struct fb_wtod_t *hash_wtod;

  u32 nr_pgs_to_copy;
  u32 *lpas_to_copy;
  u8 *data_to_copy;
};

static int make_read_request_page_mapping(struct fb_context_t *ptr_fb_context,
                                          u32 logical_page_address,
                                          u8 *ptr_page_buffer,
                                          struct fb_bio_t *ptr_fb_bio);
static int make_write_request_page_mapping(struct fb_context_t *ptr_fb_context,
                                           u32 *logical_page_address,
                                           u8 *ptr_page_buffer);
static int make_flush_request_page_mapping(void);
static int make_discard_request_page_mapping(
    struct fb_context_t *ptr_fb_context, struct bio *bio);
static int fb_wb_flush(struct fb_context_t *fb);
static struct page_mapping_table_t *create_page_mapping_table(void);
static void destroy_mapping_table(struct page_mapping_table_t *mt);
static struct fb_act_blk_mngr_t *create_act_blk_mngr(struct fb_context_t *fb);
static void destroy_act_blk_mngr(struct fb_act_blk_mngr_t *abm);

static struct fb_del_mngr_t *create_del_mngr(void);
static void destroy_del_mngr(struct fb_del_mngr_t *delm);

static int fb_background_gc(struct fb_context_t *fb) {
  return trigger_bg_gc(fb);
}

static struct fb_act_blk_mngr_t *create_act_blk_mngr(struct fb_context_t *fb) {
  struct ssd_info *ssdi = get_ssd_inf(fb);
  struct fb_act_blk_mngr_t *abm = vmalloc(sizeof(struct fb_act_blk_mngr_t));

  if (abm == NULL) {
    printk(KERN_ERR "flashbench: Allocating active block manager failed.\n");
    goto FAIL;
  }

  abm->act_blks = vmalloc(sizeof(struct flash_block *) * NUM_CHIPS);
  if (abm->act_blks == NULL) {
    printk(KERN_ERR "flashbench: Allocating active block list failed.\n");
    goto FAIL;
  }

  for (u32 bus = 0; bus < NUM_BUSES; bus++) {
    for (u32 chip = 0; chip < NUM_CHIPS_PER_BUS; chip++) {
      struct flash_block *blki = get_free_block(ssdi, bus, chip);
      if (blki == NULL) {
        printk(KERN_ERR "flashbench: Getting new active block failed.\n");
        goto FAIL;
      }

      reset_free_blk(ssdi, blki);
      set_act_blk_flag(blki, true);
      abm->act_blks[bus * NUM_CHIPS_PER_BUS + chip] = blki;
    }
  }

  abm->mru_bus = NUM_BUSES - 1;
  abm->mru_chip = NUM_CHIPS_PER_BUS - 1;
  return abm;

FAIL:
  destroy_act_blk_mngr(abm);
  return NULL;
}

static void destroy_act_blk_mngr(struct fb_act_blk_mngr_t *abm) {
  if (abm == NULL) { return; }
  if (abm->act_blks != NULL) { vfree(abm->act_blks); }
  vfree(abm);
}

struct fb_gc_mngr_t *get_gcm(struct page_mapping_context_t *ftl) {
  return ftl->gcm;
}

struct fb_act_blk_mngr_t *get_abm(struct page_mapping_context_t *ftl) {
  return ftl->abm;
}

void *create_pg_ftl(struct fb_context_t *fb) {
  struct page_mapping_context_t *ftl = vmalloc(sizeof(struct page_mapping_context_t));
  if (ftl == NULL) {
    printk(KERN_ERR
           "flashbench: Allocating mapping table failed during "
           "create_pg_ftl().\n");
    goto FAIL;
  }

  ftl->ptr_mapping_table = create_page_mapping_table();
  if (ftl->ptr_mapping_table == NULL) {
    printk(
        KERN_ERR
        "flashbench: Creating mapping table failed during create_pg_ftl().\n");
    goto FAIL;
  }

  ftl->abm = create_act_blk_mngr(fb);
  if (ftl->abm == NULL) {
    printk(KERN_ERR
           "flashbench: fb_page_mapping: Creating active block manager "
           "failed.\n");
    goto FAIL;
  }

  ftl->delm = create_del_mngr();
  if (ftl->delm == NULL) {
    printk(KERN_ERR
           "flashbench: fb_page_mapping: Creating del manager failed.\n");
    goto FAIL;
  }

  ftl->gcm = create_gc_mngr(fb);
  if (ftl->gcm == NULL) {
    printk(KERN_ERR
           "flashbench: fb_page_mapping: Creating GC manager failed.\n");
    goto FAIL;
  }

  ftl->lpas_to_discard = vmalloc(NR_MAX_LPAS_DISCARD * sizeof(u32));
  if (ftl->lpas_to_discard == NULL) {
    printk(KERN_ERR "flashbench: fb_page_mapping: Allocating lpas failed.\n");

    goto FAIL;
  }

  fb->ptr_mapping_context = ftl;

  fb->make_read_request = make_read_request_page_mapping;
  fb->make_write_request = make_write_request_page_mapping;
  fb->make_flush_request = make_flush_request_page_mapping;
  fb->make_discard_request = make_discard_request_page_mapping;
  fb->background_gc = fb_background_gc;
  fb->wb_flush = fb_wb_flush;

  init_completion(&ftl->mapping_context_lock);
  complete(&ftl->mapping_context_lock);

  return ftl;

FAIL:
  destroy_pg_ftl(ftl);

  return NULL;
}

static void init_delm(struct fb_del_mngr_t *delm) {
  delm->nr_btod = 0;
  HASH_CLEAR(hh, delm->hash_btod);
  delm->hash_btod = NULL;

  delm->nr_wtod = 0;
  HASH_CLEAR(hh, delm->hash_wtod);
  delm->hash_wtod = NULL;

  delm->nr_pgs_to_copy = 0;
}

static struct fb_del_mngr_t *create_del_mngr(void) {
  struct fb_del_mngr_t *delm = vmalloc(sizeof(struct fb_del_mngr_t));
  if (delm == NULL) {
    printk(KERN_ERR "flashbench: Allocating DEL manager failed.\n");
    goto FAIL;
  }

  delm->btod = vmalloc(sizeof(struct fb_btod_t) * NUM_BTODS);
  if (delm->btod == NULL) {
    printk(KERN_ERR "flashbench: Allocating DEL block list failed.\n");
    goto FAIL;
  }

  for (u32 i = 0; i < NUM_BTODS; ++i) { delm->btod[i].blki = NULL; }

  delm->wtod = vmalloc(sizeof(struct fb_wtod_t) * NUM_WTODS);
  if (delm->wtod == NULL) {
    printk(KERN_ERR "flashbench: Allocating DEL WL list failed.\n");
    goto FAIL;
  }

  for (u32 i = 0; i < NUM_WTODS; i++) {
    delm->wtod[i].wl_idx = -1;
    delm->wtod[i].bus = -1;
    delm->wtod[i].chip = -1;
  }

  delm->ppas = vmalloc(sizeof(u32) * NR_MAX_LPAS_DISCARD);
  if (delm->ppas == NULL) {
    printk(KERN_ERR "flashbench: Allocating PPA list failed.\n");
    goto FAIL;
  }

  delm->lpas_to_copy = vmalloc(sizeof(u32) * NR_MAX_LPGS_COPY);
  if (delm->lpas_to_copy == NULL) {
    printk(KERN_ERR "flashbench: Allocating LPA list failed.\n");
    goto FAIL;
  }

  delm->data_to_copy = vmalloc(sizeof(u8) * LOGICAL_PAGE_SIZE * NR_MAX_LPGS_COPY);
  if (delm->data_to_copy == NULL) {
    printk(KERN_ERR "flashbench: Allocating valid page buffer failed.\n");
    goto FAIL;
  }

  delm->hash_btod = NULL;
  delm->hash_wtod = NULL;

  init_delm(delm);

  return delm;

FAIL:
  destroy_del_mngr(delm);

  return NULL;
}

static void destroy_del_mngr(struct fb_del_mngr_t *delm) {
  if (delm == NULL) { return; }

  if (delm->hash_btod != NULL) { HASH_CLEAR(hh, delm->hash_btod); }
  if (delm->hash_wtod != NULL) { HASH_CLEAR(hh, delm->hash_wtod); }
  if (delm->btod != NULL) { vfree(delm->btod); }
  if (delm->wtod != NULL) { vfree(delm->wtod); }
  if (delm->lpas_to_copy != NULL) { vfree(delm->lpas_to_copy); }
  if (delm->data_to_copy != NULL) { vfree(delm->data_to_copy); }
  vfree(delm);
}

static void _fb_del_invalidate_pgs(struct fb_context_t *fb, u32 nr_reqs,
                                  u32 *req_lpas) {
  struct page_mapping_context_t *ftl = get_ftl(fb);
  struct fb_del_mngr_t *delm = ftl->delm;

  init_delm(delm);

  // TODO: Race condition! wait_for_completion + reinit_completion is NOT
  // atomic!
  wait_for_completion(&ftl->mapping_context_lock);
  reinit_completion(&ftl->mapping_context_lock);

  for (u32 loop = 0; loop < nr_reqs; loop++) {
    // 1. invalidate the lpa
    //  - access to the L2P mapping - then we can know the physical page to lock
    //  - change the status of physical page (4-KiB) to invalid
    delm->ppas[loop] = invalidate_lpg(fb, req_lpas[loop]);
    // this is all we have to do here.
  }

  complete(&ftl->mapping_context_lock);
}

void fb_del_invalid_data(struct fb_context_t *fb, struct fb_bio_t *fb_bio) {
  // 1. Invalidate all LPAs in the request
  _fb_del_invalidate_pgs(fb, fb_bio->req_count, fb_bio->lpas);
}

void destroy_pg_ftl(struct page_mapping_context_t *ftl) {
  if (ftl == NULL) { return; }

  destroy_act_blk_mngr(ftl->abm);
  destroy_gc_mngr(ftl->gcm);
  destroy_mapping_table(ftl->ptr_mapping_table);
  destroy_del_mngr(ftl->delm);

  if (ftl->lpas_to_discard != NULL) { vfree(ftl->lpas_to_discard); }
  vfree(ftl);
}

static int make_read_request_page_mapping(struct fb_context_t *ptr_fb_context,
                                          u32 logical_page_address,
                                          u8 *ptr_page_buffer,
                                          struct fb_bio_t *ptr_fb_bio) {

  struct page_mapping_context_t *ptr_mapping_context = ptr_fb_context->ptr_mapping_context;

  wait_for_completion(&ptr_mapping_context->mapping_context_lock);
  reinit_completion(&ptr_mapping_context->mapping_context_lock);

  u32 bus, chip, block, page;
  u32 physical_page_address = get_mapped_physical_address(
      ptr_fb_context, logical_page_address, &bus, &chip, &block, &page);

  if (physical_page_address == PAGE_UNMAPPED) {
    complete(&ptr_mapping_context->mapping_context_lock);
    return -1;
  }

  perf_inc_nr_page_reads();

  u32 page_offset = LP_PAGE_MASK & page;
  page = page >> LP_PAGE_SHIFT;
  u8 page_bitmap[NR_LP_IN_PP] = {0};
  page_bitmap[page_offset] = 1;

  vdevice_read(ptr_fb_context->ptr_vdevice, bus, chip, block, page, page_bitmap,
               ptr_page_buffer, ptr_fb_bio);

  complete(&ptr_mapping_context->mapping_context_lock);

  return 0;
}

static int is_fgc_needed(struct fb_context_t *fb, u8 bus, u8 chip) {
  for (u8 chip_idx = chip; chip_idx < NUM_CHIPS_PER_BUS; chip_idx++) {
    for (u8 bus_idx = bus; bus_idx < NUM_BUSES; bus_idx++) {
      if ((get_curr_gc_block(fb, bus, chip) == NULL) &&
          (get_free_block(get_ssd_inf(fb), bus, chip) == NULL)) {
        return true;
      }
    }
  }

  return false;
}

static int make_write_request_page_mapping(struct fb_context_t *fb, u32 *lpa,
                                           u8 *src) {
  struct page_mapping_context_t *ftl = get_ftl(fb);

  wait_for_completion(&ftl->mapping_context_lock);
  reinit_completion(&ftl->mapping_context_lock);

  u8 bus, chip;
  get_next_bus_chip(fb, &bus, &chip);
  // Check foreground GC condition, check if the GC block is null
  if (is_fgc_needed(fb, bus, chip) && trigger_gc_page_mapping(fb) == -1) {
    printk(KERN_ERR "flashbench: fb_page_mapping: Foreground GC failed.\n");
    goto FAILED;
  }

  get_next_bus_chip(fb, &bus, &chip);

  u32 blk, pg;
  if ((alloc_new_page(fb, bus, chip, &blk, &pg)) == -1) {
    if (trigger_gc_page_mapping(fb) == -1) {
      printk(KERN_ERR "flashbench: fb_page_mapping: Foreground GC failed.\n");
      goto FAILED;
    }

    get_next_bus_chip(fb, &bus, &chip);

    if ((alloc_new_page(fb, bus, chip, &blk, &pg)) == -1) {
      printk(KERN_ERR
             "flashbench: fb_page_mapping: There is not sufficient space.\n");
      goto FAILED;
    }
  }

  perf_inc_nr_wordline_prog_fg();

  vdevice_write(get_vdev(fb), bus, chip, blk, pg, src, NULL);

  if (map_logical_to_physical(fb, lpa, bus, chip, blk, pg) == -1) {
    printk(KERN_ERR
           "flashbench: fb_page_mapping: Mapping LPA to PPA failed.\n");
    goto FAILED;
  }

  set_prev_bus_chip(fb, bus, chip);

  update_act_blk(fb, bus, chip);

  complete(&ftl->mapping_context_lock);

  return 0;

FAILED:
  complete(&ftl->mapping_context_lock);
  return -1;
}

static int make_flush_request_page_mapping(void) {
  printk(KERN_INFO "flashbench: FLUSH\n");
  return 0;
}

static int make_discard_request_page_mapping(struct fb_context_t *fb,
                                             struct bio *bio) {
  struct page_mapping_context_t *ftl = get_ftl(fb);
  struct fb_wb *wb = fb->wb;

  u64 sec_start = (bio->bi_iter.bi_sector + 7) & (~(7));
  u64 sec_end = (bio->bi_iter.bi_sector + (bio->bi_iter.bi_size >> 9)) & (~(7));

  u64 lpa_start = sec_start >> 3;
  u64 nr_lpgs = (sec_start < sec_end) ? ((sec_end - sec_start) >> 3) : 0;

  perf_inc_nr_discard_reqs();
  perf_inc_nr_discard_lpgs(nr_lpgs);

  for (u32 lp_loop = 0; lp_loop < nr_lpgs; lp_loop++) {
    fb_rm_buf_pg(wb, lpa_start + lp_loop);
  }

  u32 bio_loop = 0;
  for (u32 lp_loop = 0; lp_loop < nr_lpgs; lp_loop++) {
    ftl->lpas_to_discard[bio_loop] = lpa_start + lp_loop;

    if (bio_loop == (NR_MAX_LPAS_DISCARD - 1)) {
      _fb_del_invalidate_pgs(fb, NR_MAX_LPAS_DISCARD, ftl->lpas_to_discard);
      bio_loop = 0;
    } else {
      bio_loop++;
    }
  }

  if (bio_loop != 0) {
    _fb_del_invalidate_pgs(fb, bio_loop, ftl->lpas_to_discard);
  }

  return 0;
}

static void destroy_mapping_table(struct page_mapping_table_t *mt) {
  if (mt == NULL) { return; }
  if (mt->mappings != NULL) { vfree(mt->mappings); }
  kfree(mt);
}

static struct page_mapping_table_t *create_page_mapping_table(void) {
  struct page_mapping_table_t *ptr_mapping_table = kmalloc(sizeof(struct page_mapping_table_t), GFP_ATOMIC);
  if (ptr_mapping_table == NULL) {
    printk(KERN_ERR
           "flashbench: Allocating mapping table failed during "
           "create_page_mapping_table().\n");
    goto FAIL_ALLOC_TABLE;
  }

  ptr_mapping_table->nr_entries = ((NUM_BUSES * NUM_CHIPS_PER_BUS *
        NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK * PHYSICAL_PAGE_SIZE /
        LOGICAL_PAGE_SIZE) * CFACTOR_PERCENT) / 100;

  ptr_mapping_table->mappings = vmalloc(sizeof(u32) * ptr_mapping_table->nr_entries);
  if (ptr_mapping_table->mappings == NULL) {
    printk(KERN_ERR
           "flashbench: Allocating mapping entries failed during "
           "create_page_mapping_table().\n");
    goto FAIL_ALLOC_ENTRIES;
  }

  for (u32 i = 0; i < ptr_mapping_table->nr_entries; i++) {
    ptr_mapping_table->mappings[i] = PAGE_UNMAPPED;
  }

  return ptr_mapping_table;

FAIL_ALLOC_ENTRIES:
  if (ptr_mapping_table != NULL) {
    kfree(ptr_mapping_table);
  }
FAIL_ALLOC_TABLE:
  return NULL;
}

void print_blk_mgmt(struct fb_context_t *fb) {
  u8 bus, chip;
  get_next_bus_chip(fb, &bus, &chip);
  printk(KERN_INFO "flashbench: Current bus: %u, chip: %u\n", bus, chip);

  for (u8 chip = 0; chip < NUM_CHIPS_PER_BUS; chip++) {
    for (u8 bus = 0; bus < NUM_BUSES; bus++) {
      printk(
          KERN_INFO
          "flashbench: Block Status b(%u), c(%u) - act: %s(%u), gc: %s, free: "
          "%u, used: "
          "%u, dirt: %u\n",
          bus, chip, (get_curr_active_block(fb, bus, chip) == NULL) ? "X" : "O",
          (get_curr_active_block(fb, bus, chip) == NULL)
              ? 0
              : get_nr_free_pgs(get_curr_active_block(fb, bus, chip)),
          (get_curr_gc_block(fb, bus, chip) == NULL) ? "X" : "O",
          get_nr_free_blks_in_chip(get_chip_info(get_ssd_inf(fb), bus, chip)),
          get_nr_used_blks_in_chip(get_chip_info(get_ssd_inf(fb), bus, chip)),
          get_nr_dirt_blks_in_chip(get_chip_info(get_ssd_inf(fb), bus, chip)));
    }
  }
}

static int fb_wb_flush(struct fb_context_t *fb) {
  struct fb_wb *wb = fb->wb;

  u32 lpas[NR_LP_IN_PP];

  while (fb_get_pgs_to_write(wb, NR_LP_IN_PP, lpas, wb->pg_buf) != -1) {
    if (make_write_request_page_mapping(fb, lpas, wb->pg_buf) == -1) {
      printk(KERN_ERR "flashbench: Handling a write request filed.\n");
      return -1;
    }

    fb_rm_written_pgs(wb);
  }

  return 0;
}
