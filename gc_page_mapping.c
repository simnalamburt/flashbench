#include "rust/libflashbench.h"

#include "ftl_algorithm_page_mapping.h"
#include "main.h"
#include "page_mapping_function.h"
#include "util.h"

int trigger_bg_gc(struct fb_context_t *fb) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);
  struct fb_gc_mngr_t *gcm = get_gcm(ftl);

  u8 bus, chip;

  wait_for_completion(&ftl->mapping_context_lock);
  reinit_completion(&ftl->mapping_context_lock);

  get_prev_bus_chip(fb, &bus, &chip);

  if (get_curr_active_block(fb, bus, chip) == NULL) {
    // there is no free block:
    // as long as a free block exists, it is used for active block
    if (fb_bgc_prepare_act_blks(fb) != 0) {
      printk(KERN_ERR "flashbench: Preparing active blocks for BGC failed.\n");
      goto FAIL;
    }

    perf_inc_nr_gc_trigger_bg();

    // BGC should be performed in a step by step manner
    complete(&ftl->mapping_context_lock);
    return 0;
  }

  init_gcm(gcm);

  if (fb_bgc_set_vic_blks(fb) != 0) {
    printk(KERN_ERR "flashbench: Setting victim blocks for BGC failed.\n");
    goto FAIL;
  }

  if (fb_bgc_read_valid_pgs(fb) != 0) {
    printk(KERN_ERR "flashbench: Reading valid pages for BGC failed.\n");
    goto FAIL;
  }

  if (gcm->nr_pgs_to_copy == 0) {
    complete(&ftl->mapping_context_lock);
    return 0;
  }

  perf_inc_nr_gc_trigger_bg();

  if (prog_valid_pgs_to_gc_blks(fb) != 0) {
    printk(KERN_ERR "flashbench: Writing valid pages for BGC failed.\n");
    goto FAIL;
  }

  if (update_gc_blks(fb) != 0) {
    printk(KERN_ERR "flashbench: Updating GC blocks for BGC failed.\n");
    goto FAIL;
  }

  complete(&ftl->mapping_context_lock);
  return 0;

FAIL:
  complete(&ftl->mapping_context_lock);
  return -1;
}
