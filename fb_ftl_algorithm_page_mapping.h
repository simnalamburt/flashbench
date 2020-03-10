#include "libs/uthash.h"
#define PAGE_UNMAPPED -1

struct page_mapping_table_t {
	u32 nr_entries;
	u32* mappings;
};

struct fb_act_blk_mngr_t {
	struct flash_block **act_blks;

	u32 mru_bus;
	u32 mru_chip;
};

struct fb_btod_t { // block to del
	struct flash_block *blki;

	UT_hash_handle hh;
};

struct fb_wtod_t { // wordline to del
	u32 wl_idx;
	u32 bus;
	u32 chip;

	UT_hash_handle hh;
};

struct fb_del_mngr_t {

	// list of pages (blocks) to lock - physical address base
	// list of pages to copy - logical address base
	// data buffers for pages to copy
	u32 *ppas;

	u32 nr_btod;
	struct fb_btod_t *btod;
	struct fb_btod_t *hash_btod;

	u32 nr_wtod;
	struct fb_wtod_t *wtod;
	struct fb_wtod_t *hash_wtod;

	u32 nr_pgs_to_copy;
	u32 *lpas_to_copy;
	u8 *data_to_copy;

};

struct fb_gc_mngr_t {
	struct flash_block **gc_blks;

	struct flash_block **vic_blks;
	u32 *first_valid_pg;

	u32 *lpas_to_copy;
	u8 *data_to_copy;

	u32 nr_pgs_to_copy;
};

struct page_mapping_context_t
{
	struct completion mapping_context_lock;

	struct page_mapping_table_t *ptr_mapping_table;

	struct fb_act_blk_mngr_t *abm;

	struct fb_gc_mngr_t *gcm;

	struct fb_del_mngr_t *delm;

	u32 *lpas_to_discard;
};

void *create_pg_ftl (struct fb_context_t *fb);
void destroy_pg_ftl (struct page_mapping_context_t *ftl);
struct fb_gc_mngr_t *get_gcm (struct page_mapping_context_t *ftl);
struct fb_act_blk_mngr_t *get_abm (struct page_mapping_context_t *ftl);
void print_blk_mgmt (struct fb_context_t *fb);
int fb_del_invalid_data (struct fb_context_t *fb, struct fb_bio_t  *fb_bio);

struct fb_del_mngr_t *get_delm (struct page_mapping_context_t *ftl);
