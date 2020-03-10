#include "uthash.h"

struct fb_wb_pg_t {
	uint32_t lpa;
	uint8_t *data;

	int wflag;

	struct fb_wb_pg_t *prev, *next;

	UT_hash_handle hh;
};

struct fb_wb {
	uint32_t pg_size;
	uint32_t nr_entries;

	struct fb_wb_pg_t* free_pgs; 	//page list head
	struct fb_wb_pg_t* writing_pgs;
	struct fb_wb_pg_t* buf_pgs;

	struct fb_wb_pg_t* hash_pgs;	//hashed page

	uint8_t *pg_buf;

	struct completion wb_lock;
	//spinlock_t wb_lock;
};

#define fb_wb_get_pg_buf(a) (a->pg_buf)

struct fb_wb *fb_create_write_buffer (uint32_t nr_max_entries, uint32_t pg_size);

void fb_destroy_write_buffer (struct fb_wb *wb);

uint32_t fb_get_nr_pgs_in_wb (struct fb_wb *wb, int lock);

int fb_get_pg_data (struct fb_wb *wb, uint32_t lpa, uint8_t* dest);

int fb_get_pgs_to_write (struct fb_wb *wb, uint32_t nr_pgs, uint32_t *lpas, uint8_t* dest);

int fb_put_pg (struct fb_wb *wb, uint32_t lpa, uint8_t* src);

void fb_rm_written_pgs (struct fb_wb *wb);

void fb_rm_buf_pg (struct fb_wb *wb, uint32_t lpa);
