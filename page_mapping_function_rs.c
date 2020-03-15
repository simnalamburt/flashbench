#include "page_mapping_function.h"
#include "ftl_algorithm_page_mapping.h"
#include "main.h"
#include "ssd_info.h"
#include "vdevice_rs.h"


static int __map_logical_to_physical(struct fb_context_t *ptr_fb_context,
                              u32 logical_page_address, u32 bus, u32 chip,
                              u32 block, u32 page, u8 page_offset);

void get_prev_bus_chip(struct fb_context_t *fb, u8 *bus, u8 *chip) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);
  struct fb_act_blk_mngr_t *abm = get_abm(ftl);

  *bus = abm->mru_bus;
  *chip = abm->mru_chip;
}

int alloc_new_page(struct fb_context_t *fb, u8 bus, u8 chip, u32 *blk,
                   u32 *pg) {
  struct ssd_info *ssdi = get_ssd_inf(fb);

  struct flash_block *blki;

  if ((blki = get_curr_active_block(fb, bus, chip)) == NULL) {
    if ((blki = get_free_block(ssdi, bus, chip)) == NULL) {
      return -1;
    } else {
      reset_free_blk(ssdi, blki);
      set_curr_active_block(fb, bus, chip, blki);
    }
  }

  *blk = blki->no_block;
  *pg = NUM_PAGES_PER_BLOCK - get_nr_free_pgs(blki);

  *blk = blki->no_block;
  *pg = NUM_PAGES_PER_BLOCK - get_nr_free_pgs(blki);

  return 0;
}

int map_logical_to_physical(struct fb_context_t *ptr_fb_context,
                            u32 *logical_page_address, u32 bus, u32 chip,
                            u32 block, u32 page) {
  u8 lp_loop;

  int ret = -1;

  for (lp_loop = 0; lp_loop < NR_LP_IN_PP; lp_loop++) {
    if ((ret = __map_logical_to_physical(ptr_fb_context,
                                         logical_page_address[lp_loop], bus,
                                         chip, block, page, lp_loop)) == -1)
      break;
  }

  return ret;
}

void update_act_blk(struct fb_context_t *fb, u8 bus, u8 chip) {
  struct flash_block *blki = get_curr_active_block(fb, bus, chip);

  if (get_nr_free_pgs(blki) == 0) {
    struct ssd_info *ssdi = get_ssd_inf(fb);
    set_act_blk_flag(blki, false);
    set_used_blk(ssdi, blki);

    if ((blki = get_free_block(ssdi, bus, chip)) != NULL)
      reset_free_blk(ssdi, blki);

    set_curr_active_block(fb, bus, chip, blki);
  }
}

u32 invalidate_lpg(struct fb_context_t *fb, u32 lpa) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);
  struct ssd_info *ssdi = get_ssd_inf(fb);

  u32 ppa = get_mapped_ppa(ftl, lpa);

  if (ppa != (u32)PAGE_UNMAPPED) {
    u32 bus, chip, blk, pg;
    struct flash_block *blki;
    struct flash_page *pgi;

    convert_to_ssd_layout(ppa, &bus, &chip, &blk, &pg);

    blki = get_block_info(ssdi, bus, chip, blk);
    pgi = get_page_info(ssdi, bus, chip, blk, (pg >> LP_PAGE_SHIFT));

    set_pg_status(pgi, (pg & LP_PAGE_MASK), PAGE_STATUS_INVALID);
    inc_nr_invalid_lps_in_blk(blki);
    dec_nr_valid_lps_in_blk(blki);

    if (inc_nr_invalid_lps(pgi) == NR_LP_IN_PP) {
      dec_nr_valid_pgs(blki);
      if (inc_nr_invalid_pgs(blki) == NUM_PAGES_PER_BLOCK) {
        reset_used_blk(ssdi, blki);

        if (get_curr_gc_block(fb, bus, chip) == NULL)
          set_curr_gc_block(fb, bus, chip, blki);
        else
          set_dirt_blk(ssdi, blki);
      }
    }

    set_mapped_ppa(ftl, lpa, PAGE_UNMAPPED);
  }

  return ppa;
}

int __map_logical_to_physical(struct fb_context_t *fb, u32 lpa, u32 bus,
                              u32 chip, u32 blk, u32 pg, u8 lp_ofs) {
  struct page_mapping_context_t *ftl =
      (struct page_mapping_context_t *)get_ftl(fb);
  struct ssd_info *ssdi = get_ssd_inf(fb);

  struct flash_block *blki = get_block_info(ssdi, bus, chip, blk);
  struct flash_page *pgi = get_page_info(ssdi, bus, chip, blk, pg);

  if (lpa != (u32)PAGE_UNMAPPED) {
    invalidate_lpg(fb, lpa);
    inc_nr_valid_lps_in_blk(blki);
    set_pg_status(pgi, lp_ofs, PAGE_STATUS_VALID);
    set_mapped_ppa(ftl, lpa,
                   convert_to_physical_address(
                       bus, chip, blk, ((pg << LP_PAGE_SHIFT) | lp_ofs)));
  } else {
    inc_nr_invalid_lps_in_blk(blki);
    inc_nr_invalid_lps(pgi);
    set_pg_status(pgi, lp_ofs, PAGE_STATUS_INVALID);
  }

  set_mapped_lpa(pgi, lp_ofs, lpa);

  if (lp_ofs == NR_LP_IN_PP - 1) {
    inc_nr_valid_pgs(blki);
    dec_nr_free_pgs(blki);
  }

  return 0;
}
