#include "uthash.h"
#define PAGE_UNMAPPED -1

struct page_mapping_table_t {
	uint32_t nr_entries;
	uint32_t* mappings;
};

typedef struct {
	struct flash_block_t **act_blks;

	uint32_t mru_bus;
	uint32_t mru_chip;
} fb_act_blk_mngr_t;

typedef struct fb_btod_s { // block to del
	struct flash_block_t *blki;

	UT_hash_handle hh;
} fb_btod_t;

typedef struct fb_wtod_s { // wordline to del
	uint32_t wl_idx;
	uint32_t bus;
	uint32_t chip;

	UT_hash_handle hh;
} fb_wtod_t;

typedef struct {

	// list of pages (blocks) to lock - physical address base
	// list of pages to copy - logical address base
	// data buffers for pages to copy
	uint32_t *ppas;

	uint32_t nr_btod;
	fb_btod_t *btod;
	fb_btod_t *hash_btod;

	uint32_t nr_wtod;
	fb_wtod_t *wtod;
	fb_wtod_t *hash_wtod;

	uint32_t nr_pgs_to_copy;
	uint32_t *lpas_to_copy;
	uint8_t *data_to_copy;

} fb_del_mngr_t;

typedef struct {
	struct flash_block_t **gc_blks;

	struct flash_block_t **vic_blks;
	uint32_t *first_valid_pg;

	uint32_t *lpas_to_copy;
	uint8_t *data_to_copy;

	uint32_t nr_pgs_to_copy;
} fb_gc_mngr_t;

typedef struct page_mapping_context_t
{
	struct completion mapping_context_lock;

	struct page_mapping_table_t *ptr_mapping_table;

	fb_act_blk_mngr_t *abm;

	fb_gc_mngr_t *gcm;

	fb_del_mngr_t *delm;

	uint32_t *lpas_to_discard;
} fb_pg_ftl_t;

void *create_pg_ftl (fb_t *fb);
void destroy_pg_ftl (fb_pg_ftl_t *ftl);
inline fb_gc_mngr_t *get_gcm (fb_pg_ftl_t *ftl);
inline fb_act_blk_mngr_t *get_abm (fb_pg_ftl_t *ftl);
void print_blk_mgmt (fb_t *fb);
int fb_del_invalid_data (fb_t *fb, fb_bio_t  *fb_bio);

inline fb_del_mngr_t *get_delm (fb_pg_ftl_t *ftl);

