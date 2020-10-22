#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "uthash/utlist.h"

#include "option.h"
#include "write_buffer.h"

static void fb_destroy_wb_entry(struct fb_wb_pg_t *wb_pg) {
  if (!wb_pg) { return; }
  if (wb_pg->data) vfree(wb_pg->data);

  vfree(wb_pg);
}

static struct fb_wb_pg_t *fb_create_wb_entry(u32 pg_size) {
  struct fb_wb_pg_t *wb_pg = vmalloc(sizeof(struct fb_wb_pg_t));
  if (!wb_pg) {
    printk(KERN_ERR "flashbench: Allocating buffered page failed.\n");
    goto FAIL;
  }

  wb_pg->data = vmalloc(sizeof(u8) * pg_size);
  if (!wb_pg->data) {
    printk(KERN_ERR "flashbench: Allocating data buffer failed.\n");
    goto FAIL;
  }

  wb_pg->lpa = -1;
  wb_pg->wflag = false;

  return wb_pg;

FAIL:
  fb_destroy_wb_entry(wb_pg);

  return NULL;
}

static void fb_wb_set_free_pg(struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
  DL_APPEND(wb->free_pgs, wb_pg);
}

static void fb_wb_reset_free_pg(struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
  DL_DELETE(wb->free_pgs, wb_pg);
}

static void fb_wb_set_writing_pg(struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
  DL_APPEND(wb->writing_pgs, wb_pg);
  wb_pg->wflag = true;
}

static void fb_wb_reset_writing_pg(struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
  DL_DELETE(wb->writing_pgs, wb_pg);
  wb_pg->wflag = false;
}

static u32 fb_wb_set_buf_pg(struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
  DL_APPEND(wb->buf_pgs, wb_pg);
  return ++wb->nr_entries;
}

static struct fb_wb_pg_t *fb_wb_get_buf_pg(struct fb_wb *wb, u32 lpa) {
  struct fb_wb_pg_t *wb_pg = NULL;

  HASH_FIND(hh, wb->hash_pgs, &lpa, sizeof(u32), wb_pg);

  return wb_pg;
}

static u32 fb_wb_reset_buf_pg(struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
  DL_DELETE(wb->buf_pgs, wb_pg);
  return --wb->nr_entries;
}

struct fb_wb *fb_create_write_buffer(u32 nr_max_entries, u32 pg_size) {
  struct fb_wb *wb = vmalloc(sizeof(struct fb_wb));
  if (!wb) {
    printk(KERN_ERR "flashbench: Allocating write buffer failed.\n");
    goto FAIL;
  }

  wb->pg_size = pg_size;
  wb->nr_entries = 0;
  wb->free_pgs = NULL;
  wb->writing_pgs = NULL;
  wb->buf_pgs = NULL;
  wb->hash_pgs = NULL;

  for (u32 i = 0; i < nr_max_entries; i++) {
    struct fb_wb_pg_t *wb_pg = fb_create_wb_entry(pg_size);
    if (!wb_pg) {
      printk(KERN_ERR "flashbench: Creating wb entry failed.\n");
      goto FAIL;
    }

    fb_wb_set_free_pg(wb, wb_pg);
  }

  wb->pg_buf = vmalloc(sizeof(u8) * PHYSICAL_PAGE_SIZE);
  if (!wb->pg_buf) {
    printk(KERN_ERR "flashbench: Allocating page buffer failed.\n");
    goto FAIL;
  }

  init_completion(&wb->wb_lock);
  complete(&wb->wb_lock);

  return wb;

FAIL:
  fb_destroy_write_buffer(wb);

  return NULL;
}

void fb_destroy_write_buffer(struct fb_wb *wb) {
  if (!wb) { return; }

  if (wb->free_pgs) {
    for (;;) {
      struct fb_wb_pg_t *wb_pg = wb->free_pgs;
      if (!wb_pg) { break; }
      fb_wb_reset_free_pg(wb, wb_pg);
      fb_destroy_wb_entry(wb_pg);
    }
  }

  if (wb->buf_pgs) {
    for (;;) {
      struct fb_wb_pg_t *wb_pg = wb->buf_pgs;
      if (!wb_pg) { break; }
      fb_wb_reset_buf_pg(wb, wb_pg);
      HASH_DEL(wb->hash_pgs, wb_pg);
      fb_destroy_wb_entry(wb_pg);
    }
  }

  if (wb->writing_pgs) {
    for (;;) {
      struct fb_wb_pg_t *wb_pg = wb->writing_pgs;
      if (!wb_pg) { break; }
      fb_wb_reset_writing_pg(wb, wb_pg);
      HASH_DEL(wb->hash_pgs, wb_pg);
      fb_destroy_wb_entry(wb_pg);
    }
  }

  vfree(wb);
}

static u32 fb_get_nr_pgs_in_wb(struct fb_wb *wb, int lock) {
  if (lock) {
    wait_for_completion(&wb->wb_lock);
    reinit_completion(&wb->wb_lock);
  }

  u32 ret = wb->nr_entries;

  if (lock) {
    complete(&wb->wb_lock);
  }

  return ret;
}

int fb_get_pg_data(struct fb_wb *wb, u32 lpa, u8 *dest) {
  int ret = -1;

  wait_for_completion(&wb->wb_lock);
  reinit_completion(&wb->wb_lock);

  struct fb_wb_pg_t *wb_pg = fb_wb_get_buf_pg(wb, lpa);
  if (wb_pg) {
    memcpy(dest, wb_pg->data, wb->pg_size);
    ret = 0;
  }

  complete(&wb->wb_lock);

  return ret;
}

int fb_get_pgs_to_write(struct fb_wb *wb, u32 nr_pgs, u32 *lpas, u8 *dest) {
  int ret = -1;

  wait_for_completion(&wb->wb_lock);
  reinit_completion(&wb->wb_lock);

  if (fb_get_nr_pgs_in_wb(wb, false) >= nr_pgs) {
    for (u32 i = 0; i < nr_pgs; i++) {
      struct fb_wb_pg_t *wb_pg = wb->buf_pgs;
      fb_wb_reset_buf_pg(wb, wb_pg);
      fb_wb_set_writing_pg(wb, wb_pg);

      memcpy(dest + (i * wb->pg_size), wb_pg->data, wb->pg_size);
      lpas[i] = wb_pg->lpa;
    }

    ret = 0;
  }

  complete(&wb->wb_lock);

  return ret;
}

int fb_put_pg(struct fb_wb *wb, u32 lpa, u8 *src) {
  struct fb_wb_pg_t *wb_pg = fb_wb_get_buf_pg(wb, lpa);
  if (!wb_pg) {
    wb_pg = wb->free_pgs;
    if (!wb_pg) { return -1; }

    fb_wb_reset_free_pg(wb, wb_pg);

    wb_pg->lpa = lpa;
    HASH_ADD(hh, wb->hash_pgs, lpa, sizeof(u32), wb_pg);
  } else {
    if (wb_pg->wflag) {
      fb_wb_reset_writing_pg(wb, wb_pg);
    } else {
      fb_wb_reset_buf_pg(wb, wb_pg);
    }
  }

  memcpy(wb_pg->data, src, wb->pg_size);
  fb_wb_set_buf_pg(wb, wb_pg);

  return 0;
}

void fb_rm_written_pgs(struct fb_wb *wb) {
  for (;;) {
    struct fb_wb_pg_t *wb_pg = wb->writing_pgs;
    if (!wb_pg) { break; }
    fb_wb_reset_writing_pg(wb, wb_pg);
    HASH_DEL(wb->hash_pgs, wb_pg);

    fb_wb_set_free_pg(wb, wb_pg);
  }
}

void fb_rm_buf_pg(struct fb_wb *wb, u32 lpa) {
  struct fb_wb_pg_t *wb_pg = fb_wb_get_buf_pg(wb, lpa);
  if (!wb_pg) { return; }
  if (wb_pg->wflag) {
    fb_wb_reset_writing_pg(wb, wb_pg);
  } else {
    fb_wb_reset_buf_pg(wb, wb_pg);
  }
  HASH_DEL(wb->hash_pgs, wb_pg);

  fb_wb_set_free_pg(wb, wb_pg);
}
