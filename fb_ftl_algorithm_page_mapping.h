#include "uthash.h"
#define PAGE_UNMAPPED -1

struct page_mapping_table_t {
	uint32_t nr_entries;
	uint32_t* mappings;
};

struct fb_act_blk_mngr_t {
	struct flash_block_t **act_blks;

	uint32_t mru_bus;
	uint32_t mru_chip;
};

struct fb_btod_t { // block to del
	struct flash_block_t *blki;

	UT_hash_handle hh;
};

struct fb_wtod_t { // wordline to del
	uint32_t wl_idx;
	uint32_t bus;
	uint32_t chip;

	UT_hash_handle hh;
};

struct fb_del_mngr_t {

	// list of pages (blocks) to lock - physical address base
	// list of pages to copy - logical address base
	// data buffers for pages to copy
	uint32_t *ppas;

	uint32_t nr_btod;
	struct fb_btod_t *btod;
	struct fb_btod_t *hash_btod;

	uint32_t nr_wtod;
	struct fb_wtod_t *wtod;
	struct fb_wtod_t *hash_wtod;

	uint32_t nr_pgs_to_copy;
	uint32_t *lpas_to_copy;
	uint8_t *data_to_copy;

};

struct fb_gc_mngr_t {
	struct flash_block_t **gc_blks;

	struct flash_block_t **vic_blks;
	uint32_t *first_valid_pg;

	uint32_t *lpas_to_copy;
	uint8_t *data_to_copy;

	uint32_t nr_pgs_to_copy;
};

struct page_mapping_context_t
{
	struct completion mapping_context_lock;

	struct page_mapping_table_t *ptr_mapping_table;

	struct fb_act_blk_mngr_t *abm;

	struct fb_gc_mngr_t *gcm;

	struct fb_del_mngr_t *delm;

	uint32_t *lpas_to_discard;
};

void *create_pg_ftl (struct fb_context_t *fb);
void destroy_pg_ftl (struct page_mapping_context_t *ftl);
struct fb_gc_mngr_t *get_gcm (struct page_mapping_context_t *ftl);
struct fb_act_blk_mngr_t *get_abm (struct page_mapping_context_t *ftl);
void print_blk_mgmt (struct fb_context_t *fb);
int fb_del_invalid_data (struct fb_context_t *fb, struct fb_bio_t  *fb_bio);

struct fb_del_mngr_t *get_delm (struct page_mapping_context_t *ftl);
