#include "uthash/uthash.h"

struct fb_wb_pg_t {
	u32 lpa;
	u8 *data;

	int wflag;

	struct fb_wb_pg_t *prev, *next;

	UT_hash_handle hh;
};

struct fb_wb {
	u32 pg_size;
	u32 nr_entries;

	struct fb_wb_pg_t* free_pgs; 	//page list head
	struct fb_wb_pg_t* writing_pgs;
	struct fb_wb_pg_t* buf_pgs;

	struct fb_wb_pg_t* hash_pgs;	//hashed page

	u8 *pg_buf;

	struct completion wb_lock;
	//spinlock_t wb_lock;
};

#define fb_wb_get_pg_buf(a) (a->pg_buf)

struct fb_wb *fb_create_write_buffer (u32 nr_max_entries, u32 pg_size);

void fb_destroy_write_buffer (struct fb_wb *wb);

u32 fb_get_nr_pgs_in_wb (struct fb_wb *wb, int lock);

int fb_get_pg_data (struct fb_wb *wb, u32 lpa, u8* dest);

int fb_get_pgs_to_write (struct fb_wb *wb, u32 nr_pgs, u32 *lpas, u8* dest);

int fb_put_pg (struct fb_wb *wb, u32 lpa, u8* src);

void fb_rm_written_pgs (struct fb_wb *wb);

void fb_rm_buf_pg (struct fb_wb *wb, u32 lpa);
