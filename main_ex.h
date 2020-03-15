#pragma once
#include <linux/completion.h>
#include "main.h"  // NR_MAX_REQ_BIO

struct bio;
struct fb_lbs_mngr_t;
struct ssd_info;
struct vdevice_t;
struct gendisk;
struct request_queue;
struct fb_wb;
struct task_struct;

struct fb_bio_t {
  u32 req_count;
  u32 lpas[NR_MAX_REQ_BIO];
  u8 *kpages[NR_MAX_REQ_BIO];
  struct bio *bio;

  // Initial state (state after `fb_build_bio()` call) of this completion is
  // "done"
  struct completion bio_lock;
};

struct fb_context_t {
  int err;

  void *ptr_mapping_context;

  struct fb_lbs_mngr_t *ptr_lbs_mngr;

  struct ssd_info *ptr_ssd_info;
  struct vdevice_t *ptr_vdevice;

  int (*make_flush_request)(void);
  int (*make_discard_request)(struct fb_context_t *ptr_fb_context,
                              struct bio *bio);
  int (*make_read_request)(struct fb_context_t *ptr_fb_context, u32 lpa_curr,
                           u8 *ptr_page_buffer, struct fb_bio_t *ptr_fb_bio);
  int (*make_write_request)(struct fb_context_t *ptr_fb_context, u32 *lpa_curr,
                            u8 *ptr_page_buffer);
  int (*background_gc)(struct fb_context_t *ptr_fb_context);

  int (*wb_flush)(struct fb_context_t *ptr_fb_context);

  struct gendisk *gd;
  struct request_queue *ptr_req_queue;
  struct completion dev_lock;
  int device_major_num;

  struct fb_wb *wb;  // write_buffer
  struct task_struct *ptr_wb_task;
  u32 flag_enable_wb_thread;

  u64 background_gc_time_stamp;
};
