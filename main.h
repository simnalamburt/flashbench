#pragma once
#include <linux/types.h>

enum {
  NR_MAX_REQ_BIO = 256,
};

struct ssd_info;
struct vdevice_t;
struct fb_wb;
struct fb_bio_t;
struct fb_context_t;

u32 dec_bio_req_count(struct fb_bio_t *ptr_bio);
struct fb_wb *get_write_buffer(struct fb_context_t *fb);
struct ssd_info *get_ssd_inf(struct fb_context_t *fb);
struct vdevice_t *get_vdev(struct fb_context_t *fb);
void *get_ftl(struct fb_context_t *fb);
