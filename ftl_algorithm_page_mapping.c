#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "main.h"
#include "macro.h"
#include "vdevice.h"
#include "ssd_info.h"
#include "write_buffer.h"
#include "ftl_algorithm_page_mapping.h"
#include "page_mapping_function.h"
#include "gc_page_mapping.h"
#include "util.h"
#include "uthash/utlist.h"
#include "uthash/uthash.h"


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

static int make_read_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		u32 logical_page_address,
		u8 *ptr_page_buffer,
		struct fb_bio_t *ptr_fb_bio);
static int make_write_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		u32 *logical_page_address,
		u8 *ptr_page_buffer);
static int make_flush_request_page_mapping(void);
static int make_discard_request_page_mapping(
		struct fb_context_t *ptr_fb_context,
		struct bio *bio);
static int fb_wb_flush (struct fb_context_t *fb);
static struct page_mapping_table_t *create_page_mapping_table(void);
static void destroy_mapping_table (struct page_mapping_table_t *mt);
static struct fb_act_blk_mngr_t *create_act_blk_mngr (struct fb_context_t *fb);
static void destroy_act_blk_mngr (struct fb_act_blk_mngr_t *abm);

static struct fb_del_mngr_t *create_del_mngr (void);
static void destroy_del_mngr (struct fb_del_mngr_t *delm);

static int fb_background_gc (struct fb_context_t *fb) {
	return trigger_bg_gc (fb);
}

static struct fb_act_blk_mngr_t *create_act_blk_mngr (struct fb_context_t *fb) {
	struct fb_act_blk_mngr_t *abm = NULL;
	struct ssd_info *ssdi = get_ssd_inf (fb);
	struct flash_block *blki;
	u32 bus, chip;

	if ((abm = (struct fb_act_blk_mngr_t *) vmalloc (sizeof (struct fb_act_blk_mngr_t))) == NULL) {
		printk (KERN_ERR "Allocating active block manager failed.\n");
		goto FAIL;
	}

	if ((abm->act_blks =
				(struct flash_block **) vmalloc (
					sizeof (struct flash_block*) * NUM_CHIPS)) == NULL) {
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

static void destroy_act_blk_mngr (struct fb_act_blk_mngr_t *abm) {
	if (abm != NULL) {
		if (abm->act_blks != NULL) {
			vfree (abm->act_blks);
		}
		vfree (abm);
	}
}

struct fb_gc_mngr_t *get_gcm (struct page_mapping_context_t *ftl) {
	return ftl->gcm;
}

struct fb_act_blk_mngr_t *get_abm (struct page_mapping_context_t *ftl) {
	return ftl->abm;
}

void *create_pg_ftl (struct fb_context_t* fb)
{
	struct page_mapping_context_t *ftl;

	if ((ftl = (struct page_mapping_context_t *) vmalloc (sizeof (struct page_mapping_context_t))) == NULL) {
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
			(u32 *) vmalloc (NR_MAX_LPAS_DISCARD * sizeof (u32))) == NULL) {
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

static struct fb_del_mngr_t *get_delm (struct page_mapping_context_t *ftl) {
	return ftl->delm;
}

struct fb_btod_t *fb_del_get_btod (struct fb_del_mngr_t *delm, u32 idx) {
	return &delm->btod[idx];
}

void fb_del_set_btod (struct fb_btod_t *btod, struct flash_block *blki) {
	btod->blki = blki;
}

void fb_del_get_bus_chip_btod (
		struct fb_del_mngr_t *delm, u32 idx, u32 *bus, u32 *chip) {
	*bus = delm->btod[idx].blki->no_bus;
	*chip = delm->btod[idx].blki->no_chip;
}

void fb_del_inc_nr_btod (struct fb_del_mngr_t *delm) {
	delm->nr_btod++;
}

u32 fb_del_get_nr_btod (struct fb_del_mngr_t *delm){
	return delm->nr_btod;
}

void fb_del_set_nr_btod (struct fb_del_mngr_t *delm, u32 new) {
	delm->nr_btod = new;
}

struct fb_wtod_t *fb_del_get_wtod (struct fb_del_mngr_t *delm, u32 idx) {
	return &delm->wtod[idx];
}

void fb_del_init_wtod (
		struct fb_wtod_t *wtod, u32 bus, u32 chip, u32 wl_idx) {
	wtod->wl_idx = wl_idx;
	wtod->bus = bus;
	wtod->chip = chip;
}

void fb_del_get_bus_chip_wtod (
		struct fb_del_mngr_t *delm, u32 idx, u32 *bus, u32 *chip) {
	*bus = delm->wtod[idx].bus;
	*chip = delm->wtod[idx].chip;
}

void fb_del_inc_nr_wtod (struct fb_del_mngr_t *delm) {
	delm->nr_wtod++;
}

u32 fb_del_get_nr_wtod (struct fb_del_mngr_t *delm){
	return delm->nr_wtod;
}

void fb_del_set_nr_wtod (struct fb_del_mngr_t *delm, u32 new) {
	delm->nr_wtod = new;
}

u32 fb_del_get_nr_pgs_to_copy (struct fb_del_mngr_t *delm) {
	return delm->nr_pgs_to_copy;
}

void fb_del_set_nr_pgs_to_copy (struct fb_del_mngr_t *delm, u32 new) {
	delm->nr_pgs_to_copy = new;
}

void fb_del_inc_nr_pgs_to_copy (struct fb_del_mngr_t *delm) {
	delm->nr_pgs_to_copy++;
}

void init_delm (struct fb_del_mngr_t *delm) {
	fb_del_set_nr_btod (delm, 0);
	HASH_CLEAR (hh, delm->hash_btod);
	delm->hash_btod = NULL;

	fb_del_set_nr_wtod (delm, 0);
	HASH_CLEAR (hh, delm->hash_wtod);
	delm->hash_wtod = NULL;

	fb_del_set_nr_pgs_to_copy (delm, 0);
}

static struct fb_del_mngr_t *create_del_mngr (void) {
	struct fb_del_mngr_t *delm = NULL;

	u32 i;

	if ((delm = (struct fb_del_mngr_t *) vmalloc (sizeof (struct fb_del_mngr_t))) == NULL) {
		printk (KERN_ERR "Allocating DEL manager failed.\n");
		goto FAIL;
	}

	if ((delm->btod = (struct fb_btod_t *) vmalloc (
					sizeof (struct fb_btod_t) * NUM_BTODS)) == NULL) {
		printk (KERN_ERR "Allocating DEL block list failed.\n");
		goto FAIL;
	}

	for(i = 0; i < NUM_BTODS; i++) {
		delm->btod[i].blki = NULL;
	}

	if ((delm->wtod = (struct fb_wtod_t *) vmalloc (
					sizeof (struct fb_wtod_t) * NUM_WTODS)) == NULL) {
		printk (KERN_ERR "Allocating DEL WL list failed.\n");
		goto FAIL;
	}

	for(i = 0; i < NUM_WTODS; i++) {
		fb_del_init_wtod (&delm->wtod[i], -1, -1, -1);
	}

	if ((delm->ppas =
				(u32 *) vmalloc (
					sizeof (u32) * NR_MAX_LPAS_DISCARD)) == NULL) {
		printk (KERN_ERR "Allocating PPA list failed.\n");
		goto FAIL;
	}

	if ((delm->lpas_to_copy =
				(u32 *) vmalloc (
					sizeof (u32) * NR_MAX_LPGS_COPY)) == NULL) {
		printk (KERN_ERR "Allocating LPA list failed.\n");
		goto FAIL;
	}

	if ((delm->data_to_copy =
				(u8 *) vmalloc (
					sizeof (u8) * LOGICAL_PAGE_SIZE * NR_MAX_LPGS_COPY)) == NULL) {
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


static void destroy_del_mngr (struct fb_del_mngr_t *delm) {
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


u32* fb_del_get_lpas_to_copy (struct fb_del_mngr_t *delm) {
	return delm->lpas_to_copy;
}

u8* fb_del_get_data_to_copy (struct fb_del_mngr_t *delm) {
	return delm->data_to_copy;
}

static int _fb_del_invalidate_pgs (struct fb_context_t* fb, u32 nr_reqs, u32 *req_lpas) {
	struct page_mapping_context_t *ftl = get_ftl (fb);
	struct fb_del_mngr_t *delm = get_delm (ftl);

	u32 loop = 0;
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
	struct page_mapping_context_t *ftl = get_ftl (fb);
	struct fb_del_mngr_t *delm = get_delm (ftl);

	u32 loop;
	u32 nr_pgs_to_copy = fb_del_get_nr_pgs_to_copy (delm);

	u32 *lpas = fb_del_get_lpas_to_copy (delm);
	u8 *data = fb_del_get_data_to_copy (delm);

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

int fb_del_invalidate_pgs (struct fb_context_t* fb, struct fb_bio_t* fb_bio) {
	return _fb_del_invalidate_pgs (fb, fb_bio->req_count, fb_bio->lpas);
}

int fb_del_invalid_data (struct fb_context_t *fb, struct fb_bio_t  *fb_bio) {
	// 1. Invalidate all LPAs in the request
	fb_del_invalidate_pgs (fb, fb_bio);

	return 0;
}


void destroy_pg_ftl (struct page_mapping_context_t *ftl)
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
		u32 logical_page_address,
		u8 *ptr_page_buffer,
		struct fb_bio_t *ptr_fb_bio)
{
	u32 bus, chip, block, page, page_offset;
	u32 physical_page_address;
	u8 page_bitmap[NR_LP_IN_PP] = {0};

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

static int is_fgc_needed (struct fb_context_t *fb, u8 bus, u8 chip) {

	u8 bus_idx, chip_idx;

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
		u32 *lpa,
		u8 *src)
{
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);

	u8 bus, chip;
	u32 blk, pg;


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

static u32 *get_lpas_to_discard (struct page_mapping_context_t *ftl) {
	return ftl->lpas_to_discard;
}

static void set_lpa_to_discard (
		struct page_mapping_context_t *ftl, u32 idx, u32 new) {
	ftl->lpas_to_discard[idx] = new;
}

static int make_discard_request_page_mapping (struct fb_context_t *fb, struct bio *bio) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_wb *wb = get_write_buffer (fb);
	u64 sec_start, sec_end, lpa_start, nr_lpgs;
	u32 lp_loop, bio_loop;

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
	u32 i;
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
				(u32 *)vmalloc(sizeof(u32) * ptr_mapping_table->nr_entries)) == NULL)
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
	u8 bus, chip;

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

static int fb_wb_flush (struct fb_context_t *fb) {
	struct fb_wb *wb = get_write_buffer (fb);

	u32 lpas[NR_LP_IN_PP];

	while (fb_get_pgs_to_write (wb, NR_LP_IN_PP, lpas, wb->pg_buf) != -1) {
		if (make_write_request_page_mapping (
					fb, lpas, wb->pg_buf) == -1) {

			fb_print_err ("Handling a write request filed.\n");

			return -1;
		}

		fb_rm_written_pgs (wb);
	}

	return 0;
}
