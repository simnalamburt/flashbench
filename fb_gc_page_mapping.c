#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

#include "fb.h"
#include "fb_option.h"
#include "fb_vdevice.h"
#include "fb_ssd_info.h"
#include "fb_write_buffer.h"
#include "fb_ftl_algorithm_page_mapping.h"
#include "fb_page_mapping_function.h"
#include "fb_gc_page_mapping.h"
#include "fb_util.h"
#include "utlist.h"


static inline void init_gcm (struct fb_gc_mngr_t *gcm) {
	gcm->nr_pgs_to_copy = 0;
}

static inline fb_blk_inf_t *get_vic_blk (
		struct fb_gc_mngr_t *gcm, uint32_t bus, uint32_t chip) {
	return gcm->vic_blks[bus * NUM_CHIPS_PER_BUS + chip];
}

static inline void set_vic_blk (
		struct fb_gc_mngr_t *gcm, uint32_t bus, uint32_t chip, fb_blk_inf_t *blki) {
	gcm->vic_blks[bus * NUM_CHIPS_PER_BUS + chip] = blki;
}

inline uint32_t find_first_valid_pg (fb_blk_inf_t *blki, uint32_t start_pg) {
	fb_pg_inf_t *pgi;
	uint32_t pg;

	for (pg = start_pg ; pg < NUM_PAGES_PER_BLOCK ; pg++) {
		pgi = get_pgi_from_blki (blki, pg);

		if (get_nr_invalid_lps (pgi) < NR_LP_IN_PP)
			break;
	}

	return pg;
}

inline void set_first_valid_pg (
		struct fb_gc_mngr_t *gcm, uint32_t bus, uint32_t chip, uint32_t pg) {
	gcm->first_valid_pg[bus * NUM_CHIPS_PER_BUS + chip] = pg;
}

inline uint32_t get_first_valid_pg (struct fb_gc_mngr_t *gcm, uint32_t bus, uint32_t chip) {
	return gcm->first_valid_pg[bus * NUM_CHIPS_PER_BUS + chip];
}

static fb_blk_inf_t* select_vic_blk_from_used (
		fb_ssd_inf_t *ssdi,
		uint32_t bus, uint32_t chip) {
	fb_blk_inf_t *vic_blki = NULL, *blki;
	uint32_t nr_max_invalid_lpgs = 0, nr_invalid_lpgs;

	blk_list_for_each (get_used_block (ssdi, bus, chip), blki) {
		if ((nr_invalid_lpgs = get_nr_invalid_lps_in_blk (blki)) > nr_max_invalid_lpgs) {
			nr_max_invalid_lpgs = nr_invalid_lpgs;
			vic_blki = blki;
		}
	}

	return vic_blki;
}

static fb_blk_inf_t* select_vic_blk_greedy (
		fb_ssd_inf_t *ssdi,
		uint32_t bus, uint32_t chip) {
	fb_blk_inf_t *vic_blki = NULL, *blki;
	uint32_t nr_max_invalid_lpgs = 0, nr_invalid_lpgs;

	if ((vic_blki = get_dirt_block (ssdi, bus, chip)) != NULL)
		return vic_blki;

	blk_list_for_each (get_used_block (ssdi, bus, chip), blki) {
		if ((nr_invalid_lpgs = get_nr_invalid_lps_in_blk (blki)) > nr_max_invalid_lpgs) {
			nr_max_invalid_lpgs = nr_invalid_lpgs;
			vic_blki = blki;
		}
	}

	return vic_blki;
}

static int set_vic_blks (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);
	fb_ssd_inf_t *ssdi = get_ssd_inf (fb);

	fb_blk_inf_t *blki;

	uint32_t bus, chip;

	for (bus = 0 ; bus < NUM_BUSES ; bus++) {
		for( chip = 0 ; chip < NUM_CHIPS_PER_BUS ; chip++) {
			if ((get_curr_gc_block (fb, bus, chip) != NULL) &&
					(get_curr_active_block (fb, bus, chip) != NULL))
				blki = NULL;
			else {
				if ((blki = select_vic_blk_greedy (ssdi, bus, chip)) == NULL) {
					printk (KERN_ERR "[FlashBench] fb_gc_page_mapping: There is no avaiable victim block.\n");
					return -1;
				}

				if (get_nr_valid_lps_in_blk (blki) == 0)
					blki = NULL;
				else {
					gcm->nr_pgs_to_copy += get_nr_valid_lps_in_blk (blki);
					set_first_valid_pg (gcm, bus, chip,
							find_first_valid_pg (blki, 0));
				}
			}

			set_vic_blk (gcm, bus, chip, blki);
		}
	}

	return 0;
}

static void get_valid_pgs_in_vic_blks (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);
	struct vdevice_t *vdev = get_vdev (fb);

	fb_blk_inf_t *blki;
	fb_pg_inf_t *pgi;

	uint32_t nr_read_pgs = 0;
	uint32_t *ptr_lpa = gcm->lpas_to_copy;
	uint8_t *ptr_data = gcm->data_to_copy;
	uint8_t lp_bitmap[NR_LP_IN_PP];
	uint8_t bus, chip;
	uint32_t pg, lp, nr_pgs_to_read;

	while (gcm->nr_pgs_to_copy > nr_read_pgs) {
		for (pg = 0 ; pg < NUM_PAGES_PER_BLOCK ; pg++) {
			for (chip = 0 ; chip < NUM_CHIPS_PER_BUS ; chip++) {
				for (bus = 0 ; bus < NUM_BUSES ; bus++) {
					blki = get_vic_blk (gcm, bus, chip);

					if (blki == NULL)
						continue;

					pgi = get_pgi_from_blki (blki, pg);

					if ((nr_pgs_to_read = NR_LP_IN_PP - get_nr_invalid_lps (pgi)) > 0) {
						for (lp = 0 ; lp < NR_LP_IN_PP ; lp++) {
							if (get_pg_status (pgi, lp) == PAGE_STATUS_VALID) {
								*ptr_lpa = get_mapped_lpa (pgi, lp);
								lp_bitmap[lp] = 1;
								ptr_lpa++;
							} else {
								lp_bitmap[lp] = 0;
							}
						}

						perf_inc_nr_page_reads ();

						vdevice_read(vdev, bus, chip, get_blk_idx (blki), pg,
								lp_bitmap, ptr_data, NULL);

						ptr_data += nr_pgs_to_read * LOGICAL_PAGE_SIZE;
						nr_read_pgs += nr_pgs_to_read;
					}
				}
			}
		}
	}
}

static int prog_valid_pgs_to_gc_blks (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);
	struct vdevice_t *vdev = get_vdev (fb);

	int32_t nr_pgs_to_prog = gcm->nr_pgs_to_copy;
	uint32_t *ptr_lpa = gcm->lpas_to_copy;
	uint8_t *ptr_data = gcm->data_to_copy;
	uint8_t bus, chip;
	uint32_t blk, pg, lp;

	while (nr_pgs_to_prog > 0) {
		get_next_bus_chip (fb, &bus, &chip);

		if (alloc_new_page (fb, bus, chip, &blk, &pg) == -1) {
			printk (KERN_ERR "Wrong active block handling\n");
			print_blk_mgmt (fb);
			return -1;
		}

		if (nr_pgs_to_prog < NR_LP_IN_PP) {
			for (lp = nr_pgs_to_prog ; lp < NR_LP_IN_PP ; lp++) {
				*(ptr_lpa + lp) = PAGE_UNMAPPED;
			}
		}

		perf_inc_nr_wordline_prog_bg();

		vdevice_write(vdev, bus, chip, blk, pg, ptr_data, NULL);

		if (map_logical_to_physical(fb, ptr_lpa, bus, chip, blk, pg) == -1) {
			printk (KERN_ERR "[FlashBench] Mapping L2P in GC failed.\n");
			return -1;
		}

		ptr_lpa += NR_LP_IN_PP;
		ptr_data += PHYSICAL_PAGE_SIZE;
		nr_pgs_to_prog -= NR_LP_IN_PP;

		set_prev_bus_chip(fb, bus, chip);
		update_act_blk(fb, bus, chip);
	}

	return 0;
}

static int prepare_act_blks (struct fb_context_t *fb) {
	struct vdevice_t *vdev = get_vdev (fb);

	fb_blk_inf_t *blki;

	uint8_t bus, chip;

	for (chip = 0 ; chip < NUM_CHIPS_PER_BUS ; chip++) {
		for (bus = 0 ; bus < NUM_BUSES ; bus++) {
			if (get_curr_active_block (fb, bus, chip) != NULL)
				continue;

			if ((blki = get_curr_gc_block (fb, bus, chip)) == NULL) {
				printk (KERN_ERR "Wrong GC block handling\n");
				print_blk_mgmt (fb);
				return -1;
			}

			vdevice_erase(vdev, bus, chip, get_blk_idx (blki), NULL);

			perf_inc_nr_blk_erasures();

			init_blk_inf (blki);
			inc_bers_cnt (blki);

			set_curr_active_block (fb, bus, chip, blki);
			set_curr_gc_block (fb, bus, chip, NULL);
		}
	}

	return 0;
}

static int update_gc_blks (struct fb_context_t *fb) {
	fb_blk_inf_t *gc_blki;

	uint32_t bus, chip;

	for (bus = 0 ; bus < NUM_BUSES ; bus++) {
		for (chip = 0 ; chip < NUM_CHIPS_PER_BUS ; chip++) {
			if (get_curr_gc_block (fb, bus, chip) == NULL) {
				if ((gc_blki = get_dirt_block (get_ssd_inf (fb), bus, chip)) == NULL) {
					if (get_curr_active_block (fb, bus, chip) == NULL) {
						printk (KERN_ERR "Wrong block management handling\n");
						print_blk_mgmt (fb);
						return -1;
					}
				} else {
					reset_dirt_blk (get_ssd_inf (fb), gc_blki);
					set_curr_gc_block (fb, bus, chip, gc_blki);
				}
			}
		}
	}

	return 0;
}

struct fb_gc_mngr_t *create_gc_mngr (struct fb_context_t *fb) {
	fb_ssd_inf_t *ssdi = get_ssd_inf (fb);
	struct fb_gc_mngr_t *gcm = NULL;
	fb_blk_inf_t *blki;
	uint32_t bus, chip;

	if ((gcm = (struct fb_gc_mngr_t *) vmalloc (sizeof (struct fb_gc_mngr_t))) == NULL) {
		printk (KERN_ERR "Allocating GC manager failed.\n");
		goto FAIL;
	}

	if ((gcm->gc_blks =
				(fb_blk_inf_t **) vmalloc (
					sizeof (fb_blk_inf_t *) * NUM_CHIPS)) == NULL) {
		printk (KERN_ERR "Allocating GC block list failed.\n");
		goto FAIL;
	}

	if ((gcm->vic_blks =
				(fb_blk_inf_t **) vmalloc (
					sizeof (fb_blk_inf_t *) * NUM_CHIPS)) == NULL) {
		printk (KERN_ERR "Allocating victim block list failed.\n");
		goto FAIL;
	}

	if ((gcm->lpas_to_copy =
				(uint32_t *) vmalloc (
					sizeof (uint32_t) *
					NUM_CHIPS * NUM_PAGES_PER_BLOCK * NR_LP_IN_PP)) == NULL) {
		printk (KERN_ERR "Allocating LPA list failed.\n");
		goto FAIL;
	}

	if ((gcm->data_to_copy =
				(uint8_t *) vmalloc (
					sizeof (uint32_t) *
					NUM_CHIPS * NUM_PAGES_PER_BLOCK * NR_LP_IN_PP *
					LOGICAL_PAGE_SIZE)) == NULL) {
		printk (KERN_ERR "Allocating valid page buffer failed.\n");
		goto FAIL;
	}

	if ((gcm->first_valid_pg =
				(uint32_t *) vmalloc (sizeof (uint32_t) * NUM_CHIPS)) == NULL) {
		printk (KERN_ERR "Allocating page_offset failed.\n");
		goto FAIL;
	}

	init_gcm (gcm);

	for (bus = 0; bus < NUM_BUSES ; bus++) {
		for (chip = 0; chip < NUM_CHIPS_PER_BUS ; chip++) {
			if ((blki = get_free_block (ssdi, bus, chip)) == NULL) {
				printk (KERN_ERR "Getting new gc block failed.\n");
				goto FAIL;
			}

			reset_free_blk (ssdi, blki);
			set_rsv_blk_flag (blki, TRUE);
			gcm->gc_blks[bus * NUM_CHIPS_PER_BUS + chip] = blki;
		}
	}

	return gcm;

FAIL:
	destroy_gc_mngr (gcm);

	return NULL;
}

void destroy_gc_mngr (struct fb_gc_mngr_t *gcm) {
	if (gcm != NULL) {
		if (gcm->gc_blks != NULL)
			vfree (gcm->gc_blks);
		if (gcm->vic_blks != NULL)
			vfree (gcm->vic_blks);
		if (gcm->lpas_to_copy != NULL)
			vfree (gcm->lpas_to_copy);
		if (gcm->data_to_copy != NULL)
			vfree (gcm->data_to_copy);

		vfree(gcm);
	}
}

int trigger_gc_page_mapping (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);

	// initialize GC context
	init_gcm (gcm);

	// 1. erase GC blocks and set them as active blocks
	if (prepare_act_blks (fb) != 0) {
		printk (KERN_ERR "[FlashBench] Preparing GC blocks failed.\n");
		return -1;
	}

	// 2. select a victim block for every parallel unit
	if (set_vic_blks (fb) != 0) {
		printk (KERN_ERR "[FlashBench] Setting victim blocks failed.\n");
		return -1;
	}

	// 3. read valid pages from victim blocks
	// JS - here is the function
	get_valid_pgs_in_vic_blks (fb);

	// 4. write read data to other pages
	if (prog_valid_pgs_to_gc_blks (fb) != 0) {
		printk (KERN_ERR "[FlashBench] Writing valid data failed.\n");
		return -1;
	}

	// 5. set GC blocks with dirt blocks
	if (update_gc_blks (fb) != 0) {
		printk (KERN_ERR "[FlahsBench] Updating GC blocks failed.\n");
		return -1;
	}

	return 0;
}

int fb_bgc_prepare_act_blks (struct fb_context_t *fb) {
	fb_ssd_inf_t *ssdi = get_ssd_inf (fb);
	fb_blk_inf_t *blki;

	uint8_t bus, chip;
	uint32_t i;

	for (i = 0 ; i < NUM_CHIPS ; i++) {
		get_next_bus_chip (fb, &bus, &chip);

		if ((blki = get_curr_gc_block (fb, bus, chip)) == NULL) {
			printk (KERN_ERR "Wrong BGC block handling\n");
			print_blk_mgmt (fb);
			return -1;
		}

		vdevice_erase (get_vdev (fb), bus, chip, get_blk_idx (blki), NULL);

		perf_inc_nr_blk_erasures();

		init_blk_inf (blki);
		inc_bers_cnt (blki);

		if (get_curr_active_block (fb, bus, chip) == NULL)
			set_curr_active_block (fb, bus, chip, blki);
		else
			set_free_blk (ssdi, blki);

		if ((blki = get_dirt_block (ssdi, bus, chip)) != NULL)
			reset_dirt_blk (ssdi, blki);

		set_curr_gc_block (fb, bus, chip, blki);

		set_prev_bus_chip (fb, bus, chip);
	}

	return 0;
}


static int fb_bgc_set_vic_blks (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	fb_ssd_inf_t *ssdi = get_ssd_inf (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);

	fb_chip_inf_t *chipi;
	fb_blk_inf_t *blki;

	uint8_t bus, chip;

	/* if necesary, find a new victim block for each chip */
	for (bus = 0 ; bus < NUM_BUSES ; bus++) {
		for (chip = 0 ; chip < NUM_CHIPS_PER_BUS ; chip++) {
			//if (get_nr_used_blks_in_chip (
			//			get_chip_info (ssdi, bus, chip)) > BGC_TH_UBLK) {
			chipi = get_chip_info (ssdi, bus, chip);

			if ((get_nr_dirt_blks_in_chip (chipi) +
					get_nr_free_blks_in_chip (chipi)) < BGC_TH_NR_BLKS) {
				/* victim block exists */
				if ((blki = get_vic_blk (gcm, bus, chip)) != NULL) {
					set_first_valid_pg (gcm, bus, chip,
							find_first_valid_pg (blki,
								get_first_valid_pg (gcm, bus, chip)));

					if (get_first_valid_pg (gcm, bus, chip) == NUM_PAGES_PER_BLOCK) {
						set_vic_blk (gcm, bus, chip, NULL);
					}
					else {
						continue;
					}
				}

				/* find a new one */
				if ((blki = select_vic_blk_from_used (ssdi, bus, chip)) == NULL) {
					printk (KERN_ERR "Wrong block managment\n");
					print_blk_mgmt (fb);
					return -1;
				}

				set_vic_blk (gcm, bus, chip, blki);
				set_first_valid_pg (gcm, bus, chip, find_first_valid_pg (blki, 0));
			} else {
				set_vic_blk (gcm, bus, chip, NULL);
				set_first_valid_pg (gcm, bus, chip, 0);
			}
		}
	}

	return 0;
}

int fb_bgc_read_valid_pgs (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);

	fb_blk_inf_t *vic_blki = NULL;
	fb_pg_inf_t *pgi = NULL;

	uint8_t bus, chip, lp;
	uint32_t pg, nr_pgs_to_read;
	uint8_t lp_bitmap[NR_LP_IN_PP];

	uint32_t *ptr_lpas = gcm->lpas_to_copy;
	uint8_t *ptr_data = gcm->data_to_copy;

	uint32_t i;

	for (i = 0 ; i < NUM_CHIPS ; i++) {
		get_next_bus_chip (fb, &bus, &chip);

		if ((vic_blki = get_vic_blk (gcm, bus, chip)) == NULL) {
			set_prev_bus_chip (fb, bus, chip);
			continue;
		}

		pg = get_first_valid_pg (gcm, bus, chip);
		pgi = get_pgi_from_blki (vic_blki, pg);

		if ((nr_pgs_to_read = NR_LP_IN_PP - get_nr_invalid_lps (pgi)) == 0) {
			printk (KERN_ERR "Wrong page offset in victim block\n");
			return -1;
		}

		gcm->nr_pgs_to_copy += nr_pgs_to_read;

		for (lp = 0 ; lp < NR_LP_IN_PP ; lp++) {
			if (get_pg_status (pgi, lp) == PAGE_STATUS_VALID) {
				*ptr_lpas = get_mapped_lpa (pgi, lp);
				lp_bitmap[lp] = 1;
				ptr_lpas++;
			} else {
				lp_bitmap[lp] = 0;
			}
		}

		perf_inc_nr_page_reads ();

		vdevice_read (get_vdev (fb),
				bus, chip, get_blk_idx (vic_blki), pg,
				lp_bitmap, ptr_data, NULL);

		ptr_data += nr_pgs_to_read * LOGICAL_PAGE_SIZE;

		set_prev_bus_chip (fb, bus, chip);
	}

	return 0;
}

int fb_bgc_write_valid_pgs (struct fb_context_t *fb) {

	return prog_valid_pgs_to_gc_blks (fb);
}

int trigger_bg_gc (struct fb_context_t *fb) {
	struct page_mapping_context_t *ftl = (struct page_mapping_context_t *) get_ftl (fb);
	struct fb_gc_mngr_t *gcm = get_gcm (ftl);

	uint8_t bus, chip;

	wait_for_completion (&ftl->mapping_context_lock);
	reinit_completion (&ftl->mapping_context_lock);

	get_prev_bus_chip (fb, &bus, &chip);

	if (get_curr_active_block (fb, bus, chip) == NULL) {
		// there is no free block:
		// as long as a free block exists, it is used for active block
		if (fb_bgc_prepare_act_blks (fb) != 0) {
			printk (KERN_ERR "Preparing active blocks for BGC failed.\n");
			goto FAIL;
		}

		perf_inc_nr_gc_trigger_bg ();

		// BGC should be performed in a step by step manner
		complete (&ftl->mapping_context_lock);
		return 0;
	}

	init_gcm (gcm);

	if (fb_bgc_set_vic_blks (fb) != 0) {
		printk (KERN_ERR "Setting victim blocks for BGC failed.\n");
		goto FAIL;
	}


	if (fb_bgc_read_valid_pgs (fb) != 0) {
		printk (KERN_ERR "Reading valid pages for BGC failed.\n");
		goto FAIL;
	}

	if (gcm->nr_pgs_to_copy == 0) {
		complete (&ftl->mapping_context_lock);
		return 0;
	}

	perf_inc_nr_gc_trigger_bg ();

	if (fb_bgc_write_valid_pgs (fb) != 0) {
		printk (KERN_ERR "Writing valid pages for BGC failed.\n");
		goto FAIL;
	}

	if (update_gc_blks (fb) != 0) {
		printk (KERN_ERR "Updating GC blocks for BGC failed.\n");
		goto FAIL;
	}

	complete (&ftl->mapping_context_lock);
	return 0;

FAIL:
	complete (&ftl->mapping_context_lock);
	return -1;
}
