#include <linux/module.h>
#include <linux/slab.h>

#include "fb.h"
#include "fb_option.h"
#include "fb_vdevice.h"
#include "fb_ssd_info.h"
#include "fb_ftl_algorithm_page_mapping.h"
#include "fb_page_mapping_function.h"

int __map_logical_to_physical(
		struct fb_context_t *ptr_fb_context,
		u32 logical_page_address,
		u32 bus,
		u32 chip,
		u32 block,
		u32 page,
		u8 page_offset);

u32 get_mapped_physical_address(
		struct fb_context_t *ptr_fb_context,
		u32 logical_page_address,
		u32 *ptr_bus,
		u32 *ptr_chip,
		u32 *ptr_block,
		u32 *ptr_page)
{
	struct page_mapping_context_t *ptr_mapping_context =
		(struct page_mapping_context_t *) ptr_fb_context->ptr_mapping_context;
	u32 physical_page_address = PAGE_UNMAPPED;

	if(is_valid_address_range(logical_page_address) == FALSE)
	{
		printk(KERN_ERR "[FlashBench] fb_page_mapping: Invalid logical page range (%d)\n", logical_page_address);
		goto FINISH;
	}

	if((physical_page_address = ptr_mapping_context->ptr_mapping_table->mappings[logical_page_address]) != PAGE_UNMAPPED)
	{
		convert_to_ssd_layout(physical_page_address, ptr_bus, ptr_chip, ptr_block, ptr_page);
	}

FINISH:
	return physical_page_address;
}

inline void set_prev_bus_chip(
		struct fb_context_t *ptr_fb_context,
		u8 bus,
		u8 chip)
{
	struct page_mapping_context_t *ptr_mapping_context =
		(struct page_mapping_context_t *) ptr_fb_context->ptr_mapping_context;
	struct fb_act_blk_mngr_t *abm = ptr_mapping_context->abm;

	abm->mru_bus = bus;
	abm->mru_chip = chip;
}

inline void get_prev_bus_chip (struct fb_context_t *fb, u8 *bus, u8 *chip) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_act_blk_mngr_t *abm = get_abm (ftl);

	*bus = abm->mru_bus;
	*chip = abm->mru_chip;
}

void get_next_bus_chip(
		struct fb_context_t *ptr_fb_context,
		u8 *ptr_bus,
		u8 *ptr_chip)
{
	struct page_mapping_context_t *ptr_mapping_context =
		(struct page_mapping_context_t *) ptr_fb_context->ptr_mapping_context;
	struct fb_act_blk_mngr_t *abm = ptr_mapping_context->abm;

	if (abm->mru_bus == NUM_BUSES - 1) {
		*ptr_bus = 0;
		*ptr_chip = (abm->mru_chip == NUM_CHIPS_PER_BUS - 1) ? 0 : abm->mru_chip + 1;
	} else {
		*ptr_bus = abm->mru_bus + 1;
		*ptr_chip = abm->mru_chip;
	}
}

int alloc_new_page (struct fb_context_t *fb, u8 bus, u8 chip, u32 *blk, u32 *pg) {
	struct ssd_info *ssdi = get_ssd_inf (fb);

	struct flash_block *blki;

	if ((blki = get_curr_active_block (fb, bus, chip)) == NULL) {
		if ((blki = get_free_block (ssdi, bus, chip)) == NULL) {
			return -1;
		} else {
			reset_free_blk (ssdi, blki);
			set_curr_active_block (fb, bus, chip, blki);
		}
	}

	*blk = blki->no_block;
	*pg = NUM_PAGES_PER_BLOCK - get_nr_free_pgs (blki);

	*blk = blki->no_block;
	*pg = NUM_PAGES_PER_BLOCK - get_nr_free_pgs (blki);

	return 0;
}

int map_logical_to_physical(
		struct fb_context_t *ptr_fb_context,
		u32 *logical_page_address,
		u32 bus,
		u32 chip,
		u32 block,
		u32 page) {

	u8 lp_loop;

	int ret = -1;

	for (lp_loop = 0 ; lp_loop < NR_LP_IN_PP ; lp_loop++) {
		if ((ret = __map_logical_to_physical (
						ptr_fb_context,
						logical_page_address[lp_loop],
						bus, chip, block, page, lp_loop)) == -1)
			break;
	}

	return ret;
}

void update_act_blk (struct fb_context_t* fb, u8 bus, u8 chip) {
	struct flash_block *blki = get_curr_active_block (fb, bus, chip);

	if (get_nr_free_pgs (blki) == 0) {
		struct ssd_info *ssdi = get_ssd_inf (fb);
		set_act_blk_flag (blki, FALSE);
		set_used_blk (ssdi, blki);

		if ((blki = get_free_block (ssdi, bus, chip)) != NULL)
			reset_free_blk (ssdi, blki);

		set_curr_active_block (fb, bus, chip, blki);
	}
}

inline u32 get_mapped_ppa (struct page_mapping_context_t *ftl, u32 lpa) {
	return ftl->ptr_mapping_table->mappings[lpa];
}

inline void set_mapped_ppa (struct page_mapping_context_t *ftl, u32 lpa, u32 ppa) {
	ftl->ptr_mapping_table->mappings[lpa] = ppa;
}

u32 invalidate_lpg (struct fb_context_t *fb, u32 lpa) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct ssd_info *ssdi = get_ssd_inf (fb);

	u32 ppa = get_mapped_ppa (ftl, lpa);

	if (ppa != PAGE_UNMAPPED) {
		u32 bus, chip, blk, pg;
		struct flash_block *blki; //, *vic_blki;
		struct flash_page *pgi;

		convert_to_ssd_layout(ppa, &bus, &chip, &blk, &pg);

		blki = get_block_info (ssdi, bus, chip, blk);
		pgi = get_page_info (ssdi, bus, chip, blk, (pg >> LP_PAGE_SHIFT));

		set_pg_status (pgi, (pg & LP_PAGE_MASK), PAGE_STATUS_INVALID);
		inc_nr_invalid_lps_in_blk (blki);
		dec_nr_valid_lps_in_blk (blki);

		/*
		if ((vic_blki = fb_gc_vic_blk (fb, bus, chip)) != NULL) {
			if ((get_nr_free_pgs (blki) == 0) &&
					(get_nr_invalid_lps_in_blk (vic_blki) <
					 get_nr_invalid_lps_in_blk (blki))) {
				set_vic_blk (get_gcm (ftl), bus, chip, blki);
			}
		}
		*/

		if (inc_nr_invalid_lps (pgi) == NR_LP_IN_PP) {
			dec_nr_valid_pgs (blki);
			if (inc_nr_invalid_pgs (blki) == NUM_PAGES_PER_BLOCK) {
				reset_used_blk (ssdi, blki);

				if (get_curr_gc_block (fb, bus, chip) == NULL)
					set_curr_gc_block (fb, bus, chip, blki);
				else
					set_dirt_blk (ssdi, blki);
			}
		}

		set_mapped_ppa (ftl, lpa, PAGE_UNMAPPED);
	}

	return ppa;
}

int __map_logical_to_physical(
		struct fb_context_t *fb,
		u32 lpa,
		u32 bus,
		u32 chip,
		u32 blk,
		u32 pg,
		u8 lp_ofs)
{
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct ssd_info *ssdi = get_ssd_inf (fb);

	struct flash_block *blki = get_block_info (ssdi, bus, chip, blk);
	struct flash_page *pgi = get_page_info (ssdi, bus, chip, blk, pg);

	if (lpa != PAGE_UNMAPPED) {
		invalidate_lpg (fb, lpa);
		inc_nr_valid_lps_in_blk (blki);
		set_pg_status (pgi, lp_ofs, PAGE_STATUS_VALID);
		set_mapped_ppa (ftl, lpa,
				convert_to_physical_address (
					bus, chip, blk, ((pg << LP_PAGE_SHIFT) | lp_ofs)));
	} else {
		inc_nr_invalid_lps_in_blk (blki);
		inc_nr_invalid_lps (pgi);
		set_pg_status (pgi, lp_ofs, PAGE_STATUS_INVALID);
	}

	set_mapped_lpa (pgi, lp_ofs, lpa);

	if (lp_ofs == NR_LP_IN_PP - 1) {
		inc_nr_valid_pgs (blki);
		dec_nr_free_pgs (blki);
	}

	return 0;
}

inline struct flash_block* get_curr_gc_block (struct fb_context_t *fb, u32 bus, u32 chip) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);

	return ftl->gcm->gc_blks[bus * NUM_CHIPS_PER_BUS + chip];
}

inline void  set_curr_gc_block (
		struct fb_context_t *fb, u32 bus, u32 chip, struct flash_block *blki) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);

	*(ftl->gcm->gc_blks + (bus * NUM_CHIPS_PER_BUS + chip)) = blki;

	if (blki != NULL)
		set_rsv_blk_flag (blki, TRUE);
}

inline struct flash_block* get_curr_active_block (struct fb_context_t *fb, u32 bus, u32 chip) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);

	return ftl->abm->act_blks[bus * NUM_CHIPS_PER_BUS + chip];
}

inline void set_curr_active_block (
		struct fb_context_t *fb, u32 bus, u32 chip, struct flash_block *blki) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);

	*(ftl->abm->act_blks + (bus * NUM_CHIPS_PER_BUS + chip)) = blki;

	if (blki != NULL)
		set_act_blk_flag (blki, TRUE);
}
