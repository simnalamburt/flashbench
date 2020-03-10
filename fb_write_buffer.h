#include "uthash.h"

typedef struct fb_wb_pg {
	uint32_t lpa;
	uint8_t *data;

	int wflag;

	struct fb_wb_pg *prev, *next;

	UT_hash_handle hh;
} fb_wb_pg_t;

typedef struct fb_wb {
	uint32_t pg_size;
	uint32_t nr_entries;

	fb_wb_pg_t* free_pgs; 	//page list head
	fb_wb_pg_t* writing_pgs;
	fb_wb_pg_t* buf_pgs;

	fb_wb_pg_t* hash_pgs;	//hashed page

	uint8_t *pg_buf;

	struct completion wb_lock;
	//spinlock_t wb_lock;
} fb_wb_t;

#define fb_wb_get_pg_buf(a) (a->pg_buf)

fb_wb_t *fb_create_write_buffer (uint32_t nr_max_entries, uint32_t pg_size);

void fb_destroy_write_buffer (fb_wb_t *wb);

uint32_t fb_get_nr_pgs_in_wb (fb_wb_t *wb, int lock);

int fb_get_pg_data (fb_wb_t *wb, uint32_t lpa, uint8_t* dest);

int fb_get_pgs_to_write (fb_wb_t *wb, uint32_t nr_pgs, uint32_t *lpas, uint8_t* dest);

int fb_put_pg (fb_wb_t *wb, uint32_t lpa, uint8_t* src);

void fb_rm_written_pgs (fb_wb_t *wb);

void fb_rm_buf_pg (fb_wb_t *wb, uint32_t lpa);
