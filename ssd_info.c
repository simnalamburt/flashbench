#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>

#include "fb.h"
#include "option.h"
#include "ftl_algorithm_page_mapping.h"
#include "ssd_info.h"

#include "uthash/utlist.h"

inline void set_free_blk (struct ssd_info *ssdi, struct flash_block *bi) {
	struct flash_chip *ci = get_chip_info (ssdi, get_bus_idx (bi), get_chip_idx (bi));

	DL_APPEND (ci->free_blks, bi);
	ci->nr_free_blocks++;
}

inline void reset_free_blk (struct ssd_info *ssdi, struct flash_block *bi) {
	struct flash_chip *ci = get_chip_info (ssdi, get_bus_idx (bi), get_chip_idx (bi));

	DL_DELETE (ci->free_blks, bi);
	ci->nr_free_blocks--;
}

inline struct flash_block *get_free_block (struct ssd_info *ssdi, u32 bus, u32 chip) {
	struct flash_chip *ci = get_chip_info (ssdi, bus, chip);

	return ci->free_blks;
}

inline u32 get_nr_free_blks_in_chip (struct flash_chip *ci) {
	return ci->nr_free_blocks;
}

inline void set_used_blk (struct ssd_info* ssdi, struct flash_block *bi) {
	struct flash_chip *ci = get_chip_info (ssdi, get_bus_idx (bi), get_chip_idx (bi));

	DL_APPEND (ci->used_blks, bi);
	ci->nr_used_blks++;
}

inline void reset_used_blk (struct ssd_info* ssdi, struct flash_block *bi) {
	struct flash_chip *ci = get_chip_info (ssdi, get_bus_idx (bi), get_chip_idx (bi));

	DL_DELETE (ci->used_blks, bi);
	ci->nr_used_blks--;
}

inline struct flash_block *get_used_block (struct ssd_info *ssdi, u32 bus, u32 chip) {
	struct flash_chip *ci = get_chip_info (ssdi, bus, chip);

	return ci->used_blks;
}

inline u32 get_nr_used_blks_in_chip (struct flash_chip *ci) {
	return ci->nr_used_blks;
}


inline void set_dirt_blk (struct ssd_info* ssdi, struct flash_block *bi) {
	struct flash_chip *ci = get_chip_info (ssdi, get_bus_idx (bi), get_chip_idx (bi));

	DL_APPEND (ci->dirt_blks, bi);
	ci->nr_dirt_blks++;
}

inline void reset_dirt_blk (struct ssd_info* ssdi, struct flash_block *bi) {
	struct flash_chip *ci = get_chip_info (ssdi, get_bus_idx (bi), get_chip_idx (bi));

	DL_DELETE (ci->dirt_blks, bi);
	ci->nr_dirt_blks--;
}

inline struct flash_block *get_dirt_block (struct ssd_info *ssdi, u32 bus, u32 chip) {
	struct flash_chip *ci = get_chip_info (ssdi, bus, chip);

	return ci->dirt_blks;
}

inline u32 get_nr_dirt_blks_in_chip (struct flash_chip *ci) {
	return ci->nr_dirt_blks;
}

inline int is_dirt_blk (struct flash_block *blki) {
	return (get_nr_invalid_pgs (blki) == NUM_PAGES_PER_BLOCK) ?
		TRUE : FALSE;
}


struct ssd_info *create_ssd_info(void)
{
	u32 loop_bus, loop_chip, loop_block, loop_page;

	struct ssd_info *ptr_ssd_info = NULL;

	if((ptr_ssd_info = (struct ssd_info *)kmalloc(sizeof(struct ssd_info), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] ssd_info: Allocating ssd information structure failed.\n");
		goto FAIL_ALLOC_SSD_INFO;
	}

	ptr_ssd_info->nr_buses = NUM_BUSES;
	ptr_ssd_info->nr_chips_per_bus = NUM_CHIPS_PER_BUS;
	ptr_ssd_info->nr_blocks_per_chip = NUM_BLOCKS_PER_CHIP;
	ptr_ssd_info->nr_pages_per_block = NUM_PAGES_PER_BLOCK;


	if((ptr_ssd_info->list_buses =
				(struct flash_bus *)kmalloc(sizeof(struct flash_bus) * NUM_BUSES, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] ssd_info: Allocating bus information structure failed.\n");
		goto FAIL_ALLOC_BUS_INFO;
	}

	for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
	{
		struct flash_bus *ptr_bus = &ptr_ssd_info->list_buses[loop_bus];

		/* initialize flash chip */
		if((ptr_bus->list_chips =
					(struct flash_chip *)kmalloc(sizeof(struct flash_chip) * NUM_CHIPS_PER_BUS, GFP_ATOMIC)) == NULL)
		{
			printk(KERN_ERR "[FlashBench] ssd_info: Allocating chip information structure failed.\n");
			goto FAIL_ALLOC_INFOS;
		}

		for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
		{
			struct flash_chip *ptr_chip = &ptr_bus->list_chips[loop_chip];

			init_completion(&ptr_chip->chip_status_lock);
			complete(&ptr_chip->chip_status_lock);

			ptr_chip->chip_status = IDLE;

			ptr_chip->list_blocks = NULL;
			ptr_chip->nr_free_blocks = 0;
			ptr_chip->nr_used_blks = 0;
			ptr_chip->nr_dirt_blks = 0;
			ptr_chip->nr_dirty_blocks = 0;

			ptr_chip->free_blks = NULL;
			ptr_chip->used_blks = NULL;
			ptr_chip->dirt_blks = NULL;

			/* initialize blocks */
			if((ptr_chip->list_blocks = (struct flash_block *)kmalloc(sizeof(struct flash_block) * NUM_BLOCKS_PER_CHIP, GFP_ATOMIC)) == NULL)
			{
				printk(KERN_ERR "[FlashBench] ssd_info: Allocating block information structure failed.\n");
				goto FAIL_ALLOC_INFOS;
			}

			for (loop_block = 0 ; loop_block < NUM_BLOCKS_PER_CHIP ; loop_block++)
			{
				struct flash_block *ptr_block = &ptr_chip->list_blocks[loop_block];

				ptr_block->list_pages = NULL;
				ptr_block->no_block = loop_block;
				ptr_block->no_chip = loop_chip;
				ptr_block->no_bus = loop_bus;

				ptr_block->nr_valid_log_pages = 0;
				ptr_block->nr_invalid_log_pages = 0;
				ptr_block->nr_valid_pages = 0;
				ptr_block->nr_invalid_pages = 0;
				ptr_block->nr_free_pages = NUM_PAGES_PER_BLOCK;

				ptr_block->nr_erase_cnt = 0;
				ptr_block->is_bad_block = FALSE;
				ptr_block->is_reserved_block = FALSE;
				ptr_block->is_active_block = FALSE;
				ptr_block->last_modified_time = 0;

				set_del_flag_blk (ptr_block, FALSE);
				// timestamp is set to 0 if the block is not used after initialization

				if((ptr_block->list_pages = (struct flash_page*)kmalloc(sizeof(struct flash_page) * NUM_PAGES_PER_BLOCK, GFP_ATOMIC)) == NULL)
				{
					printk(KERN_ERR "[FlashBench] ssd_info: Allocating page information structure failed.\n");
					goto FAIL_ALLOC_INFOS;
				}

				for (loop_page = 0 ; loop_page < NUM_PAGES_PER_BLOCK ; loop_page++)
				{
					struct flash_page *ptr_page = &ptr_block->list_pages[loop_page];
					u8 lp_loop;

					ptr_page->nr_invalid_log_pages = 0;
					for (lp_loop = 0 ; lp_loop < NR_LP_IN_PP ; lp_loop++) {
						ptr_page->no_logical_page_addr[lp_loop] = -1; // free page (by default)
						ptr_page->page_status[lp_loop] = PAGE_STATUS_FREE; // free page (by default)
					}
					set_del_flag_pg (ptr_page, FALSE);
				}

				set_free_blk (ptr_ssd_info, ptr_block);
			}
		}
	}

	return ptr_ssd_info;

FAIL_ALLOC_INFOS:
	if(ptr_ssd_info->list_buses != NULL)
	{
		for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
		{
			struct flash_bus *ptr_bus = &ptr_ssd_info->list_buses[loop_bus];
			if(ptr_bus->list_chips != NULL)
			{
				for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
				{
					struct flash_chip *ptr_chip = &ptr_bus->list_chips[loop_chip];
					if(ptr_chip->list_blocks != NULL)
					{
						for(loop_block = 0 ; loop_block < NUM_BLOCKS_PER_CHIP ; loop_block++)
						{
							struct flash_block *ptr_block = &ptr_chip->list_blocks[loop_block];
							if(ptr_block->list_pages != NULL)
							{
								kfree(ptr_block->list_pages);
							}
						}

						kfree(ptr_chip->list_blocks);
					}
				}
				kfree(ptr_bus->list_chips);
			}
		}
		kfree(ptr_ssd_info->list_buses);
	}

FAIL_ALLOC_BUS_INFO:
	if(ptr_ssd_info != NULL)
	{
		kfree(ptr_ssd_info);
	}

FAIL_ALLOC_SSD_INFO:
	return NULL;
}

void destroy_ssd_info (struct ssd_info* ptr_ssd_info)
{
	u32 loop_bus, loop_chip, loop_block;

	if(ptr_ssd_info != NULL)
	{
		if(ptr_ssd_info->list_buses != NULL)
		{
			for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
			{
				struct flash_bus *ptr_bus = &ptr_ssd_info->list_buses[loop_bus];
				if(ptr_bus->list_chips != NULL)
				{
					for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
					{
						struct flash_chip *ptr_chip = &ptr_bus->list_chips[loop_chip];
						if(ptr_chip->list_blocks != NULL)
						{
							for(loop_block = 0 ; loop_block < NUM_BLOCKS_PER_CHIP ; loop_block++)
							{
								struct flash_block *ptr_block = &ptr_chip->list_blocks[loop_block];
								if(ptr_block->list_pages != NULL)
								{
									kfree(ptr_block->list_pages);
								}
							}

							kfree(ptr_chip->list_blocks);
						}
					}
					kfree(ptr_bus->list_chips);
				}
			}
			kfree(ptr_ssd_info->list_buses);
		}

		kfree(ptr_ssd_info);
	}
}

struct flash_bus* get_bus_info(
		struct ssd_info *ptr_ssd_info,
		u32 bus)
{
	return &(ptr_ssd_info->list_buses[bus]);
}

struct flash_chip* get_chip_info(
		struct ssd_info *ptr_ssd_info,
		u32 bus,
		u32 chip)
{
	return &(ptr_ssd_info->list_buses[bus].list_chips[chip]);
}

struct flash_block* get_block_info(
		struct ssd_info *ptr_ssd_info,
		u32 bus,
		u32 chip,
		u32 block)
{
	return &(ptr_ssd_info->list_buses[bus].list_chips[chip].list_blocks[block]);
}

struct flash_page* get_page_info(
		struct ssd_info *ptr_ssd_info,
		u32 bus,
		u32 chip,
		u32 block,
		u32 page)
{
	return &(ptr_ssd_info->list_buses[bus].list_chips[chip].list_blocks[block].list_pages[page]);
}

inline int get_chip_status(
		struct ssd_info *ptr_ssd_info,
		u8 bus,
		u8 chip)
{
	int ret_value;
	struct flash_chip *ptr_flash_chip = get_chip_info(ptr_ssd_info, bus, chip);

	ret_value = ptr_flash_chip->chip_status;

	return ret_value;
}

inline void set_chip_status(
		struct ssd_info *ptr_ssd_info,
		u8 bus,
		u8 chip,
		int new_status)
{
	struct flash_chip *ptr_flash_chip = get_chip_info(ptr_ssd_info, bus, chip);

	ptr_flash_chip->chip_status = new_status;
}


inline u32 get_mapped_lpa (struct flash_page *pgi, u8 ofs) {
	return pgi->no_logical_page_addr[ofs];
}

inline void set_mapped_lpa (struct flash_page *pgi, u8 ofs, u32 lpa) {
	pgi->no_logical_page_addr[ofs] = lpa;
}

inline enum fb_pg_status_t get_pg_status (struct flash_page *pgi, u8 ofs) {
	return pgi->page_status[ofs];
}

inline void set_pg_status (struct flash_page *pgi, u8 ofs, enum fb_pg_status_t status) {
	pgi->page_status[ofs] = status;
}

inline u32 get_nr_invalid_lps (struct flash_page *pgi) {
	return pgi->nr_invalid_log_pages;
}

inline void set_nr_invalid_lps (struct flash_page *pgi, u32 value) {
	pgi->nr_invalid_log_pages = value;
}

inline u32 inc_nr_invalid_lps (struct flash_page *pgi) {
	return ++(pgi->nr_invalid_log_pages);
}

inline int get_del_flag_pg (struct flash_page *pgi) {
	return pgi->del_flag;
}

inline void set_del_flag_pg (struct flash_page *pgi, int flag) {
	pgi->del_flag = flag;
}

inline void init_blk_inf (struct flash_block *blki) {
	u8 lp;
	u32 pg;
	struct flash_page *pgi;

	set_act_blk_flag (blki, FALSE);
	set_rsv_blk_flag (blki, FALSE);

	set_nr_free_pgs (blki, NUM_PAGES_PER_BLOCK);
	set_nr_valid_pgs (blki, 0);
	set_nr_invalid_pgs (blki, 0);
	set_nr_valid_lps_in_blk (blki, 0);
	set_nr_invalid_lps_in_blk (blki, 0);

	set_del_flag_blk (blki, FALSE);

	for(pg = 0 ; pg < NUM_PAGES_PER_BLOCK ; pg++) {
		pgi = get_pgi_from_blki (blki, pg);

		set_nr_invalid_lps (pgi, 0);

		set_del_flag_pg (pgi, FALSE);

		for (lp = 0 ; lp < NR_LP_IN_PP ; lp++) {
			set_mapped_lpa (pgi, lp, PAGE_UNMAPPED);
			set_pg_status (pgi, lp, PAGE_STATUS_FREE);
		}
	}
}

inline u32 get_bus_idx (struct flash_block *blki) {
	return blki->no_bus;
}

inline u32 get_chip_idx (struct flash_block *blki) {
	return blki->no_chip;
}

inline u32 get_blk_idx (struct flash_block *blki) {
	return blki->no_block;
}

inline struct flash_page *get_pgi_from_blki (struct flash_block *blki, u32 pg) {
	return (blki->list_pages + pg);
}

inline u32 get_nr_valid_pgs (struct flash_block *blki) {
	return blki->nr_valid_pages;
}

inline u32 get_nr_invalid_pgs (struct flash_block *blki) {
	return blki->nr_invalid_pages;
}

inline u32 get_nr_free_pgs (struct flash_block *blki) {
	return blki->nr_free_pages;
}

inline void set_nr_valid_pgs (struct flash_block *blki, u32 value) {
	blki->nr_valid_pages = value;
}

inline void set_nr_invalid_pgs (struct flash_block *blki, u32 value) {
	blki->nr_invalid_pages = value;
}

inline void set_nr_free_pgs (struct flash_block *blki, u32 value) {
	blki->nr_free_pages = value;
}

inline u32 inc_nr_valid_pgs (struct flash_block *blki) {
	return ++(blki->nr_valid_pages);
}

inline u32 inc_nr_invalid_pgs (struct flash_block *blki) {
	return ++(blki->nr_invalid_pages);
}

inline u32 inc_nr_free_pgs (struct flash_block *blki) {
	return ++(blki->nr_free_pages);
}

inline u32 dec_nr_valid_pgs (struct flash_block *blki) {
	return --(blki->nr_valid_pages);
}

inline u32 dec_nr_invalid_pgs (struct flash_block *blki) {
	return --(blki->nr_invalid_pages);
}

inline u32 dec_nr_free_pgs (struct flash_block *blki) {
	return --(blki->nr_free_pages);
}

inline u32 get_nr_valid_lps_in_blk (struct flash_block *blki) {
	return blki->nr_valid_log_pages;
}

inline u32 get_nr_invalid_lps_in_blk (struct flash_block *blki) {
	return blki->nr_invalid_log_pages;
}

inline void set_nr_valid_lps_in_blk (struct flash_block *blki, u32 value) {
	blki->nr_valid_log_pages = value;
}

inline void set_nr_invalid_lps_in_blk (struct flash_block *blki, u32 value) {
	blki->nr_invalid_log_pages = value;
}

inline u32 inc_nr_valid_lps_in_blk (struct flash_block *blki) {
	return ++(blki->nr_valid_log_pages);
}

inline u32 inc_nr_invalid_lps_in_blk (struct flash_block *blki) {
	return ++(blki->nr_invalid_log_pages);
}

inline u32 dec_nr_valid_lps_in_blk (struct flash_block *blki) {
	return --(blki->nr_valid_log_pages);
}

inline u32 dec_nr_invalid_lps_in_blk (struct flash_block *blki) {
	return --(blki->nr_invalid_log_pages);
}

inline u32 get_bers_cnt (struct flash_block *blki) {
	return blki->nr_erase_cnt;
}

inline void set_bers_cnt (struct flash_block *blki, u32 value) {
	blki->nr_erase_cnt = value;
}

inline u32 inc_bers_cnt (struct flash_block *blki) {
	return ++(blki->nr_erase_cnt);
}

inline int is_bad_blk (struct flash_block *blki) {
	return (blki->is_bad_block);
}

inline void set_bad_blk_flag (struct flash_block *blki, int flag) {
	blki->is_bad_block = flag;
}

inline int is_rsv_blk (struct flash_block *blki) {
	return blki->is_reserved_block;
}

inline void set_rsv_blk_flag (struct flash_block *blki, int flag) {
	blki->is_reserved_block = flag;
}

inline int is_act_blk (struct flash_block *blki) {
	return blki->is_active_block;
}

inline void set_act_blk_flag (struct flash_block *blki, int flag) {
	blki->is_active_block = flag;
}

inline int get_del_flag_blk (struct flash_block *blki) {
	return blki->del_flag;
}
inline void set_del_flag_blk (struct flash_block *blki, int flag) {
	blki->del_flag = flag;
}

inline u32 get_nr_free_blks (struct flash_chip *chipi) {
	return chipi->nr_free_blocks;
}

inline void set_nr_free_blks (struct flash_chip *chipi, u32 value) {
	chipi->nr_free_blocks = value;
}

inline u32 inc_nr_free_blks (struct flash_chip *chipi) {
	return ++(chipi->nr_free_blocks);
}

inline u32 dec_nr_free_blks (struct flash_chip *chipi) {
	return --(chipi->nr_free_blocks);
}
