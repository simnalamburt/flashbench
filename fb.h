#include <linux/completion.h>
#include <linux/blkdev.h>

#include "fb_option.h"

extern struct fb_bio_t fb_bio;

#define NR_MAX_REQ_BIO 256

typedef struct {
	uint32_t req_count;
	uint32_t lpas[NR_MAX_REQ_BIO];
	uint8_t* kpages[NR_MAX_REQ_BIO];
	struct bio *bio;
	struct completion bio_lock;
	//spinlock_t bio_lock;
} fb_bio_t;

#define fb_bio_get_req_count(a) (a->req_count)
#define fb_bio_get_bio(a) (a->bio)
#define fb_bio_get_lpas(a) (a->lpas)
#define fb_bio_get_lpa(a,b) (a->lpas[b])
#define fb_bio_get_kpages(a) (a->kpages)
#define fb_bio_get_kpage(a,b) (a->kpages[b])
#define fb_bio_get_lock(a) (&a->bio_lock)

uint32_t dec_bio_req_count (fb_bio_t *ptr_bio);
uint32_t get_bio_req_count (fb_bio_t *ptr_bio);

typedef struct fb_context_t
{
	int err;

	void *ptr_mapping_context;

	struct fb_lbs_mngr_t *ptr_lbs_mngr;

	struct ssd_info_t	*ptr_ssd_info;
	struct vdevice_t 	*ptr_vdevice;

	int (*make_flush_request) 	(struct fb_context_t *ptr_fb_context, struct bio *bio);
	int (*make_discard_request) (struct fb_context_t *ptr_fb_context, struct bio *bio);
	int (*make_read_request) 	(struct fb_context_t *ptr_fb_context,
										uint32_t lpa_curr,
										uint8_t *ptr_page_buffer,
										fb_bio_t *ptr_fb_bio);
	int (*make_write_request) 	(struct fb_context_t *ptr_fb_context,
										uint32_t *lpa_curr,
										uint8_t *ptr_page_buffer,
										fb_bio_t *ptr_fb_bio);
	int (*background_gc) (struct fb_context_t *ptr_fb_context);

	int (*wb_flush) (struct fb_context_t *ptr_fb_context);

	struct gendisk *gd;
	struct request_queue *ptr_req_queue;
	struct completion dev_lock;
	//spinlock_t dev_lock;
	int device_major_num;

	struct fb_wb *wb; //write_buffer
	struct task_struct *ptr_wb_task;
	uint32_t flag_enable_wb_thread;

	uint64_t background_gc_time_stamp;
} fb_t;

struct fb_wb *get_write_buffer (fb_t *fb);
struct ssd_info_t *get_ssd_inf (fb_t *fb);
struct vdevice_t *get_vdev (fb_t *fb);
void *get_ftl (fb_t *fb);
