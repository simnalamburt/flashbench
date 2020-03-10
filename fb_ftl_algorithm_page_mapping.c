#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>


#include "fb.h"
#include "fb_macro.h"
#include "fb_vdevice.h"
#include "fb_ssd_info.h"
#include "fb_write_buffer.h"
#include "fb_ftl_algorithm_page_mapping.h"
#include "fb_page_mapping_function.h"
#include "fb_gc_page_mapping.h"
#include "fb_util.h"
#include "utlist.h"
#include "uthash.h"

static int make_read_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		uint32_t logical_page_address,
		uint8_t *ptr_page_buffer,
		struct fb_bio_t *ptr_fb_bio);
static int make_write_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		uint32_t *logical_page_address,
		uint8_t *ptr_page_buffer);
static int make_flush_request_page_mapping(void);
static int make_discard_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		struct bio *bio);
static int fb_wb_flush (struct fb_context_t *fb);
static struct page_mapping_table_t *create_page_mapping_table(void);
static void destroy_mapping_table (struct page_mapping_table_t *mt);
static fb_act_blk_mngr_t *create_act_blk_mngr (struct fb_context_t *fb);
static void destroy_act_blk_mngr (fb_act_blk_mngr_t *abm);

static fb_del_mngr_t *create_del_mngr (void);
static void destroy_del_mngr (fb_del_mngr_t *delm);

static int fb_background_gc (struct fb_context_t *fb) {
	return trigger_bg_gc (fb);
}

static fb_act_blk_mngr_t *create_act_blk_mngr (struct fb_context_t *fb) {
	fb_act_blk_mngr_t *abm = NULL;
	fb_ssd_inf_t *ssdi = get_ssd_inf (fb);
	fb_blk_inf_t *blki;
	uint32_t bus, chip;

	if ((abm = (fb_act_blk_mngr_t *) vmalloc (sizeof (fb_act_blk_mngr_t))) == NULL) {
		printk (KERN_ERR "Allocating active block manager failed.\n");
		goto FAIL;
	}

	if ((abm->act_blks =
				(fb_blk_inf_t **) vmalloc (
					sizeof (fb_blk_inf_t*) * NUM_CHIPS)) == NULL) {
		printk (KERN_ERR "Allocating active block list failed.\n");
		goto FAIL;
	}

	for (bus = 0; bus < NUM_BUSES ; bus++) {
		for (chip = 0; chip < NUM_CHIPS_PER_BUS ; chip++) {
			if ((blki = get_free_block (ssdi, bus, chip)) == NULL) {
				printk (KERN_ERR "Getting new active block failed.\n");
				goto FAIL;
			}

			reset_free_blk (ssdi, blki);
			set_act_blk_flag (blki, TRUE);
			abm->act_blks[bus * NUM_CHIPS_PER_BUS + chip] = blki;
		}
	}

	abm->mru_bus = NUM_BUSES - 1;
	abm->mru_chip = NUM_CHIPS_PER_BUS - 1;

	return abm;

FAIL:
	destroy_act_blk_mngr (abm);

	return NULL;
}

static void destroy_act_blk_mngr (fb_act_blk_mngr_t *abm) {
	if (abm != NULL) {
		if (abm->act_blks != NULL) {
			vfree (abm->act_blks);
		}
		vfree (abm);
	}
}

inline fb_gc_mngr_t *get_gcm (fb_pg_ftl_t *ftl) {
	return ftl->gcm;
}

inline fb_act_blk_mngr_t *get_abm (fb_pg_ftl_t *ftl) {
	return ftl->abm;
}

void *create_pg_ftl (struct fb_context_t* fb)
{
	fb_pg_ftl_t *ftl;

	if ((ftl = (fb_pg_ftl_t *) vmalloc (sizeof (fb_pg_ftl_t))) == NULL) {
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Allocating mapping table failed.\n");
		goto FAIL;
	}

	if((ftl->ptr_mapping_table = create_page_mapping_table()) == NULL) 	{
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Creating mapping table failed.\n");
		goto FAIL;
	}

	if ((ftl->abm = create_act_blk_mngr (fb)) == NULL) {
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Creating active block manager failed.\n");
		goto FAIL;
	}

	if ((ftl->delm = create_del_mngr ()) == NULL) {
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Creating del manager failed.\n");
		goto FAIL;
	}


	if ((ftl->gcm = create_gc_mngr (fb)) == NULL) {
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Creating GC manager failed.\n");
		goto FAIL;
	}

	if ((ftl->lpas_to_discard =
			(uint32_t *) vmalloc (NR_MAX_LPAS_DISCARD * sizeof (uint32_t))) == NULL) {
		printk (KERN_ERR "[FlashBench] fb_page_mapping: Allocating lpas failed.\n");

		goto FAIL;
	}

	fb->ptr_mapping_context = (void *) ftl;

	fb->make_read_request = make_read_request_page_mapping;
	fb->make_write_request = make_write_request_page_mapping;
	fb->make_flush_request = make_flush_request_page_mapping;
	fb->make_discard_request = make_discard_request_page_mapping;
	fb->background_gc = fb_background_gc;
	fb->wb_flush = fb_wb_flush;

	init_completion(&ftl->mapping_context_lock);
	complete(&ftl->mapping_context_lock);

	return (void *) ftl;

FAIL:
	destroy_pg_ftl (ftl);

	return NULL;
}

inline fb_del_mngr_t *get_delm (fb_pg_ftl_t *ftl) {
	return ftl->delm;
}

inline fb_btod_t *fb_del_get_btod (fb_del_mngr_t *delm, uint32_t idx) {
	return &delm->btod[idx];
}

inline void fb_del_set_btod (fb_btod_t *btod, fb_blk_inf_t *blki) {
	btod->blki = blki;
}

inline void fb_del_get_bus_chip_btod (
		fb_del_mngr_t *delm, uint32_t idx, uint32_t *bus, uint32_t *chip) {
	*bus = delm->btod[idx].blki->no_bus;
	*chip = delm->btod[idx].blki->no_chip;
}

inline void fb_del_inc_nr_btod (fb_del_mngr_t *delm) {
	delm->nr_btod++;
}

inline uint32_t fb_del_get_nr_btod (fb_del_mngr_t *delm){
	return delm->nr_btod;
}

inline void fb_del_set_nr_btod (fb_del_mngr_t *delm, uint32_t new) {
	delm->nr_btod = new;
}

inline fb_wtod_t *fb_del_get_wtod (fb_del_mngr_t *delm, uint32_t idx) {
	return &delm->wtod[idx];
}

inline void fb_del_init_wtod (
		fb_wtod_t *wtod, uint32_t bus, uint32_t chip, uint32_t wl_idx) {
	wtod->wl_idx = wl_idx;
	wtod->bus = bus;
	wtod->chip = chip;
}

/*
inline void fb_del_set_wtod_pgi (
		fb_wtod_t *wtod, fb_pg_type_t pg_type, fb_pg_inf_t *pgi) {
	wtod->pgs[pg_type] = pgi;
}
*/

inline void fb_del_get_bus_chip_wtod (
		fb_del_mngr_t *delm, uint32_t idx, uint32_t *bus, uint32_t *chip) {
	*bus = delm->wtod[idx].bus;
	*chip = delm->wtod[idx].chip;
}

inline void fb_del_inc_nr_wtod (fb_del_mngr_t *delm) {
	delm->nr_wtod++;
}

inline uint32_t fb_del_get_nr_wtod (fb_del_mngr_t *delm){
	return delm->nr_wtod;
}

inline void fb_del_set_nr_wtod (fb_del_mngr_t *delm, uint32_t new) {
	delm->nr_wtod = new;
}

inline uint32_t fb_del_get_nr_pgs_to_copy (fb_del_mngr_t *delm) {
	return delm->nr_pgs_to_copy;
}

inline void fb_del_set_nr_pgs_to_copy (fb_del_mngr_t *delm, uint32_t new) {
	delm->nr_pgs_to_copy = new;
}

inline void fb_del_inc_nr_pgs_to_copy (fb_del_mngr_t *delm) {
	delm->nr_pgs_to_copy++;
}

inline void init_delm (fb_del_mngr_t *delm) {
	fb_del_set_nr_btod (delm, 0);
	HASH_CLEAR (hh, delm->hash_btod);
	delm->hash_btod = NULL;

	fb_del_set_nr_wtod (delm, 0);
	HASH_CLEAR (hh, delm->hash_wtod);
	delm->hash_wtod = NULL;

	fb_del_set_nr_pgs_to_copy (delm, 0);
}

fb_del_mngr_t *create_del_mngr (void) {
	fb_del_mngr_t *delm = NULL;

	uint32_t i;

	if ((delm = (fb_del_mngr_t *) vmalloc (sizeof (fb_del_mngr_t))) == NULL) {
		printk (KERN_ERR "Allocating DEL manager failed.\n");
		goto FAIL;
	}

	if ((delm->btod = (fb_btod_t *) vmalloc (
					sizeof (fb_btod_t) * NUM_BTODS)) == NULL) {
		printk (KERN_ERR "Allocating DEL block list failed.\n");
		goto FAIL;
	}

	for(i = 0; i < NUM_BTODS; i++) {
		delm->btod[i].blki = NULL;
	}

	if ((delm->wtod = (fb_wtod_t *) vmalloc (
					sizeof (fb_wtod_t) * NUM_WTODS)) == NULL) {
		printk (KERN_ERR "Allocating DEL WL list failed.\n");
		goto FAIL;
	}

	for(i = 0; i < NUM_WTODS; i++) {
		fb_del_init_wtod (&delm->wtod[i], -1, -1, -1);
	}

	if ((delm->ppas =
				(uint32_t *) vmalloc (
					sizeof (uint32_t) * NR_MAX_LPAS_DISCARD)) == NULL) {
		printk (KERN_ERR "Allocating PPA list failed.\n");
		goto FAIL;
	}

	if ((delm->lpas_to_copy =
				(uint32_t *) vmalloc (
					sizeof (uint32_t) * NR_MAX_LPGS_COPY)) == NULL) {
		printk (KERN_ERR "Allocating LPA list failed.\n");
		goto FAIL;
	}

	if ((delm->data_to_copy =
				(uint8_t *) vmalloc (
					sizeof (uint8_t) * LOGICAL_PAGE_SIZE * NR_MAX_LPGS_COPY)) == NULL) {
		printk (KERN_ERR "Allocating valid page buffer failed.\n");
		goto FAIL;
	}

	delm->hash_btod = NULL;
	delm->hash_wtod = NULL;

	init_delm (delm);

	return delm;

FAIL:
	destroy_del_mngr (delm);

	return NULL;
}


void destroy_del_mngr (fb_del_mngr_t *delm) {
	if (delm != NULL) {
		if (delm->hash_btod != NULL) {
			HASH_CLEAR (hh, delm->hash_btod);
		}

		if (delm->hash_wtod != NULL) {
			HASH_CLEAR (hh, delm->hash_wtod);
		}

		if (delm->btod != NULL) {
			vfree (delm->btod);
		}

		if (delm->btod != NULL) {
			vfree (delm->wtod);
		}

		if (delm->lpas_to_copy != NULL) {
			vfree (delm->lpas_to_copy);
		}

		if (delm->data_to_copy != NULL) {
			vfree (delm->data_to_copy);
		}

		vfree (delm);
	}
}


// return:	0 if add new one,
//			1 otherwise (already exist)
void fb_del_add_blk_to_del (fb_del_mngr_t *delm, fb_blk_inf_t *blki) {
	fb_btod_t *btod = NULL;
	fb_btod_t *new = fb_del_get_btod (delm, fb_del_get_nr_btod (delm));

	HASH_FIND (hh, delm->hash_btod, &blki, sizeof (fb_blk_inf_t*), btod);

	//printk (KERN_INFO "blki: %p\n", blki);

	if (btod == NULL) {
		fb_del_set_btod (new, blki);
		HASH_ADD (hh, delm->hash_btod, blki, sizeof (fb_blk_inf_t*), new);
		fb_del_inc_nr_btod (delm);

		//printk (KERN_INFO "Blk (%p) is added (%u).\n",
				//blki, fb_del_get_nr_btod (delm));
	}

	//printk (KERN_INFO "Blk (%p) is exist (%u).\n",
			//blki, fb_del_get_nr_btod (delm));

}

// return:	0 if add new one,
//			1 otherwise (already exist)
void fb_del_add_wl_to_del (fb_del_mngr_t *delm,
		uint32_t bus, uint32_t chip, uint32_t blk, uint32_t pg) {
	fb_wtod_t *wtod = NULL;
	fb_wtod_t *new = fb_del_get_wtod (delm, fb_del_get_nr_wtod (delm));

	uint32_t wl_idx = convert_to_wl_idx (bus, chip, blk, pg);

	HASH_FIND (hh, delm->hash_wtod, &wl_idx, sizeof (uint32_t), wtod);

	//printk (KERN_INFO "pg: %u\n", pg);
	//printk (KERN_INFO "wl: %u\n", wl_idx);
	//printk (KERN_INFO "bus: %u, chip: %u\n", bus, chip);

	if (wtod == NULL) {
		fb_del_init_wtod (new, bus, chip, wl_idx);
		HASH_ADD (hh, delm->hash_wtod, wl_idx, sizeof (uint32_t), new);
		fb_del_inc_nr_wtod (delm);
		//printk (KERN_INFO "WL (%u, %u, %u, %u) is added (%u).\n",
				//new->bus, new->chip, blk, pg, fb_del_get_nr_wtod (delm));
	} else {
		new = wtod;
		//printk (KERN_INFO "WL (%u, %u, %u, %u) is exist (%u).\n",
				//new->bus, new->chip, blk, pg, fb_del_get_nr_wtod (delm));
	}
}

inline uint32_t* fb_del_get_lpas_to_copy (fb_del_mngr_t *delm) {
	return delm->lpas_to_copy;
}

inline uint8_t* fb_del_get_data_to_copy (fb_del_mngr_t *delm) {
	return delm->data_to_copy;
}

int _fb_del_invalidate_pgs (struct fb_context_t* fb, uint32_t nr_reqs, uint32_t *req_lpas) {
	fb_pg_ftl_t *ftl = get_ftl (fb);
	fb_del_mngr_t *delm = get_delm (ftl);

	uint32_t loop = 0;
	init_delm (delm);

	//printk(KERN_INFO "# of req: %u\n", nr_reqs);

	fb_lock (&ftl->mapping_context_lock);

	for (loop = 0; loop < nr_reqs ; loop++) {
		// 1. invalidate the lpa
		//  - access to the L2P mapping - then we can know the physical page to lock
		//  - change the status of physical page (4-KiB) to invalid
		delm->ppas[loop] = invalidate_lpg (fb, req_lpas[loop]);
		//printk(KERN_INFO "lpa: %u\n", req_lpas[loop]);
		// this is all we have to do here.
	}

	fb_unlock (&ftl->mapping_context_lock);

	return 0;
}

/*
int fb_del_put_live_pgs_to_wb (struct fb_context_t *fb) {
	fb_pg_ftl_t *ftl = get_ftl (fb);
	fb_del_mngr_t *delm = get_delm (ftl);

	uint32_t loop;
	uint32_t nr_pgs_to_copy = fb_del_get_nr_pgs_to_copy (delm);

	uint32_t *lpas = fb_del_get_lpas_to_copy (delm);
	uint8_t *data = fb_del_get_data_to_copy (delm);

	//printk (KERN_INFO "pgs_to_copy: %u\n", nr_pgs_to_copy);
	for (loop = 0 ; loop < nr_pgs_to_copy ; loop++) {
		if (fb_put_pg (get_write_buffer (fb), *lpas, data) == 0) {
			lpas++;
			data += LOGICAL_PAGE_SIZE;
		} else  {
			if (fb_wb_flush (fb) == -1) {
				fb_print_err ("WB flushing failed.\n");
				return -1;
			}

			loop--;
		}
	}

	return 0;
}
*/

inline int fb_del_invalidate_pgs (struct fb_context_t* fb, struct fb_bio_t* fb_bio) {
	return _fb_del_invalidate_pgs (fb, fb_bio->req_count, fb_bio->lpas);
}

int fb_del_invalid_data (struct fb_context_t *fb, struct fb_bio_t  *fb_bio) {
	// 1. Invalidate all LPAs in the request
	fb_del_invalidate_pgs (fb, fb_bio);

	return 0;
}


void destroy_pg_ftl (fb_pg_ftl_t *ftl)
{
	if (ftl != NULL) {
		destroy_act_blk_mngr (ftl->abm);
		destroy_gc_mngr (ftl->gcm);
		destroy_mapping_table (ftl->ptr_mapping_table);
		destroy_del_mngr (get_delm (ftl));

		if (ftl->lpas_to_discard != NULL) {
			vfree (ftl->lpas_to_discard);
		}

		vfree (ftl);
	}
}

static int make_read_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		uint32_t logical_page_address,
		uint8_t *ptr_page_buffer,
		struct fb_bio_t *ptr_fb_bio)
{
	uint32_t bus, chip, block, page, page_offset;
	uint32_t physical_page_address;
	uint8_t page_bitmap[NR_LP_IN_PP] = {0};

	struct page_mapping_context_t *ptr_mapping_context =
		(struct page_mapping_context_t *)ptr_fb_context->ptr_mapping_context;

	wait_for_completion(&ptr_mapping_context->mapping_context_lock);
	reinit_completion(&ptr_mapping_context->mapping_context_lock);

	physical_page_address = get_mapped_physical_address(
				ptr_fb_context, logical_page_address,
				&bus, &chip, &block, &page);

	if(physical_page_address == PAGE_UNMAPPED)
	{
		complete(&ptr_mapping_context->mapping_context_lock);
		return -1;
	}

	perf_inc_nr_page_reads();

	page_offset = LP_PAGE_MASK & page;
	page = (page >> LP_PAGE_SHIFT);
	page_bitmap[page_offset] = 1;

	vdevice_read(
			ptr_fb_context->ptr_vdevice,
			bus, chip, block, page, page_bitmap, ptr_page_buffer, ptr_fb_bio);

	complete(&ptr_mapping_context->mapping_context_lock);

	return 0;
}

static int is_fgc_needed (struct fb_context_t *fb, uint8_t bus, uint8_t chip) {

	uint8_t bus_idx, chip_idx;

	for (chip_idx = chip ; chip_idx < NUM_CHIPS_PER_BUS ; chip_idx++) {
		for (bus_idx = bus ; bus_idx < NUM_BUSES ; bus_idx++) {
			if ((get_curr_gc_block (fb, bus, chip) == NULL) &&
					(get_free_block (get_ssd_inf (fb), bus, chip) == NULL)) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static int make_write_request_page_mapping(
		struct fb_context_t *fb,
		uint32_t *lpa,
		uint8_t *src)
{
	fb_pg_ftl_t *ftl = (fb_pg_ftl_t *) get_ftl (fb);

	uint8_t bus, chip;
	uint32_t blk, pg;


	wait_for_completion(&ftl->mapping_context_lock);
	reinit_completion(&ftl->mapping_context_lock);

	get_next_bus_chip (fb, &bus, &chip);
	/* check foreground GC condition
	 * if the GC block is null */
	if (is_fgc_needed (fb, bus, chip) == TRUE) {
		if (trigger_gc_page_mapping (fb) == -1) {
			printk(KERN_ERR "[FlashBench] fb_page_mapping: Foreground GC failed.\n");
			goto FAILED;
		}
	}

	get_next_bus_chip (fb, &bus, &chip);

	if ((alloc_new_page (fb, bus, chip, &blk, &pg)) == -1) {
		if (trigger_gc_page_mapping (fb) == -1) {
			printk(KERN_ERR "[FlashBench] fb_page_mapping: Foreground GC failed.\n");
			goto FAILED;
		}

		get_next_bus_chip (fb, &bus, &chip);

		if ((alloc_new_page (fb, bus, chip, &blk, &pg)) == -1) {
			printk(KERN_ERR "[FlashBench] fb_page_mapping: There is not sufficient space.\n");
			goto FAILED;
		}
	}

	perf_inc_nr_wordline_prog_fg();

	vdevice_write (get_vdev (fb), bus, chip, blk, pg, src, NULL);

	if (map_logical_to_physical (fb, lpa, bus, chip, blk, pg) == -1) {
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Mapping LPA to PPA failed.\n");
		goto FAILED;
	}

	set_prev_bus_chip(fb, bus, chip);

	update_act_blk (fb, bus, chip);

	complete(&ftl->mapping_context_lock);

	return 0;

FAILED:
	complete(&ftl->mapping_context_lock);
	return -1;
}

static int make_flush_request_page_mapping(void)
{
	printk ("FLUSH\n");
	return 0;
}

static inline uint32_t *get_lpas_to_discard (fb_pg_ftl_t *ftl) {
	return ftl->lpas_to_discard;
}

static inline uint32_t get_lpa_to_discard (fb_pg_ftl_t *ftl, uint32_t idx) {
	return ftl->lpas_to_discard[idx];
}

static inline void set_lpa_to_discard (
		fb_pg_ftl_t *ftl, uint32_t idx, uint32_t new) {
	ftl->lpas_to_discard[idx] = new;
}

static int make_discard_request_page_mapping (struct fb_context_t *fb, struct bio *bio) {
	fb_pg_ftl_t *ftl = (fb_pg_ftl_t *) get_ftl (fb);
	fb_wb_t *wb = get_write_buffer (fb);
	uint64_t sec_start, sec_end, lpa_start, nr_lpgs;
	uint32_t lp_loop, bio_loop;

	sec_start = (bio->bi_iter.bi_sector + 7) & (~(7));
	sec_end = (bio->bi_iter.bi_sector + bio_sectors (bio)) & (~(7));

	lpa_start = sec_start >> 3;
	nr_lpgs = (sec_start < sec_end) ? ((sec_end - sec_start) >> 3) : 0;

	perf_inc_nr_discard_reqs ();
	perf_inc_nr_discard_lpgs (nr_lpgs);

	for (lp_loop = 0 ; lp_loop < nr_lpgs ; lp_loop++) {
		fb_rm_buf_pg (wb, lpa_start + lp_loop);
	}

	bio_loop = 0;
	for (lp_loop = 0 ; lp_loop < nr_lpgs ; lp_loop++) {
		set_lpa_to_discard (ftl, bio_loop, lpa_start + lp_loop);

		if (bio_loop == (NR_MAX_LPAS_DISCARD - 1)) {
			_fb_del_invalidate_pgs (
					fb, NR_MAX_LPAS_DISCARD, get_lpas_to_discard (ftl));
			bio_loop = 0;
		} else {
			bio_loop++;
		}
	}

	if (bio_loop != 0) {
		_fb_del_invalidate_pgs (fb, bio_loop, get_lpas_to_discard (ftl));
	}

	return 0;
}

static void destroy_mapping_table (struct page_mapping_table_t *mt) {
	if (mt != NULL) {
		if (mt->mappings != NULL) {
			vfree (mt->mappings);
		}

		kfree (mt);
	}
}

static struct page_mapping_table_t *create_page_mapping_table(void)
{
	uint32_t i;
	struct page_mapping_table_t *ptr_mapping_table;

	if((ptr_mapping_table = (struct page_mapping_table_t *)kmalloc(sizeof(struct page_mapping_table_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Allocating mapping table failed.\n");
		goto FAIL_ALLOC_TABLE;
	}

	ptr_mapping_table->nr_entries =
		NUM_BUSES * NUM_CHIPS_PER_BUS * NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK;
	ptr_mapping_table->nr_entries *= (PHYSICAL_PAGE_SIZE / LOGICAL_PAGE_SIZE);
	ptr_mapping_table->nr_entries = (ptr_mapping_table->nr_entries * CFACTOR_PERCENT) / 100;

	if((ptr_mapping_table->mappings =
				(uint32_t *)vmalloc(sizeof(uint32_t) * ptr_mapping_table->nr_entries)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Allocating mapping entries failed.\n");
		goto FAIL_ALLOC_ENTRIES;
	}

	for(i = 0 ; i < ptr_mapping_table->nr_entries ; i++)
	{
		ptr_mapping_table->mappings[i] = PAGE_UNMAPPED;
	}

	return ptr_mapping_table;

FAIL_ALLOC_ENTRIES:
	if(ptr_mapping_table != NULL)
	{
		kfree(ptr_mapping_table);
	}
FAIL_ALLOC_TABLE:
	return NULL;
}

void print_blk_mgmt (struct fb_context_t *fb) {
	uint8_t bus, chip;

	get_next_bus_chip (fb, &bus, &chip);

	printk (KERN_INFO "Current bus: %u, chip: %u\n", bus, chip);

	for (chip = 0 ; chip < NUM_CHIPS_PER_BUS ; chip++) {
		for (bus = 0 ; bus < NUM_BUSES ; bus ++) {
			printk (KERN_INFO "Block Status b(%u), c(%u) - act: %s(%u), gc: %s, free: %u, used: %u, dirt: %u\n",
					bus, chip,
					(get_curr_active_block (fb, bus, chip) == NULL) ? "X" : "O",
					(get_curr_active_block(fb, bus, chip) == NULL) ? 0 :
					get_nr_free_pgs (get_curr_active_block (fb, bus, chip)),
					(get_curr_gc_block (fb, bus, chip) == NULL) ? "X" : "O",
					get_nr_free_blks_in_chip (
						get_chip_info (get_ssd_inf (fb), bus, chip)),
					get_nr_used_blks_in_chip (
						get_chip_info (get_ssd_inf (fb), bus, chip)),
					get_nr_dirt_blks_in_chip (
						get_chip_info (get_ssd_inf (fb), bus, chip))
				   );
		}
	}
}

int fb_wb_flush (struct fb_context_t *fb) {
	fb_wb_t *wb = get_write_buffer (fb);

	uint32_t lpas[NR_LP_IN_PP];

	while (fb_get_pgs_to_write (wb, NR_LP_IN_PP, lpas, fb_wb_get_pg_buf (wb)) != -1) {
		if (make_write_request_page_mapping (
					fb, lpas, fb_wb_get_pg_buf (wb)) == -1) {
			fb_print_err ("Handling a write request filed.\n");

			return -1;
		}

		fb_rm_written_pgs (wb);
	}

	return 0;
}
