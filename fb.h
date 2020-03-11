#include <linux/completion.h>
#include <linux/blkdev.h>

#include "option.h"

extern struct fb_bio_t fb_bio;

#define NR_MAX_REQ_BIO 256

struct fb_bio_t {
	u32 req_count;
	u32 lpas[NR_MAX_REQ_BIO];
	u8* kpages[NR_MAX_REQ_BIO];
	struct bio *bio;
	struct completion bio_lock;
};

#define fb_bio_get_req_count(a) (a->req_count)
#define fb_bio_get_bio(a) (a->bio)
#define fb_bio_get_lpas(a) (a->lpas)
#define fb_bio_get_lpa(a,b) (a->lpas[b])
#define fb_bio_get_kpages(a) (a->kpages)
#define fb_bio_get_kpage(a,b) (a->kpages[b])
#define fb_bio_get_lock(a) (&a->bio_lock)

u32 dec_bio_req_count (struct fb_bio_t *ptr_bio);
u32 get_bio_req_count (struct fb_bio_t *ptr_bio);

struct fb_context_t {
	int err;

	void *ptr_mapping_context;

	struct fb_lbs_mngr_t *ptr_lbs_mngr;

	struct ssd_info	*ptr_ssd_info;
	struct vdevice_t 	*ptr_vdevice;

	int (*make_flush_request) 	(void);
	int (*make_discard_request) (struct fb_context_t *ptr_fb_context, struct bio *bio);
	int (*make_read_request) 	(struct fb_context_t *ptr_fb_context,
										u32 lpa_curr,
										u8 *ptr_page_buffer,
										struct fb_bio_t *ptr_fb_bio);
	int (*make_write_request) 	(struct fb_context_t *ptr_fb_context,
										u32 *lpa_curr,
										u8 *ptr_page_buffer);
	int (*background_gc) (struct fb_context_t *ptr_fb_context);

	int (*wb_flush) (struct fb_context_t *ptr_fb_context);

	struct gendisk *gd;
	struct request_queue *ptr_req_queue;
	struct completion dev_lock;
	//spinlock_t dev_lock;
	int device_major_num;

	struct fb_wb *wb; //write_buffer
	struct task_struct *ptr_wb_task;
	u32 flag_enable_wb_thread;

	u64 background_gc_time_stamp;
};

struct fb_wb *get_write_buffer (struct fb_context_t *fb);
struct ssd_info *get_ssd_inf (struct fb_context_t *fb);
struct vdevice_t *get_vdev (struct fb_context_t *fb);
void *get_ftl (struct fb_context_t *fb);
