#define IDLE 0
#define FG_MAPPING 1
#define BG_GC 2

#define blk_list_for_each(head, el) DL_FOREACH(head, el)

enum fb_pg_status_t {
	PAGE_STATUS_FREE,
	PAGE_STATUS_VALID,
	PAGE_STATUS_INVALID,
	PAGE_STATUS_DEL
};

struct flash_page {
	enum fb_pg_status_t page_status[NR_LP_IN_PP];
	uint32_t no_logical_page_addr[NR_LP_IN_PP];
	uint32_t nr_invalid_log_pages;

	int del_flag;
};

struct flash_block {
	/* block id */
	uint32_t no_block;
	uint32_t no_chip;
	uint32_t no_bus;

	/* block status */
	uint32_t nr_valid_pages;
	uint32_t nr_invalid_pages;
	uint32_t nr_free_pages;

	uint32_t nr_valid_log_pages;
	uint32_t nr_invalid_log_pages;


	/* the number of erasure operations */
	uint32_t nr_erase_cnt;

	/* for bad block management */
	int is_bad_block;

	/* for garbage collection */
	uint32_t last_modified_time;

	/* is reserved block for gc? */
	int is_reserved_block;

	/* is active block? */
	int is_active_block;

	/* for keeping the oob data (NOTE: it must be removed in real implementation) */
	struct flash_page* list_pages;

	struct flash_block *prev, *next;

	int del_flag;
};

struct flash_chip {
	struct completion chip_status_lock;

	int chip_status;
	/* the number of free blocks in a chip */
	uint32_t nr_free_blocks;
	uint32_t nr_used_blks;
	uint32_t nr_dirt_blks;

	struct flash_block *free_blks;
	struct flash_block *used_blks;
	struct flash_block *dirt_blks;

	/* the number of dirty blocks in a chip */
	uint32_t nr_dirty_blocks;

	/* block array */
	struct flash_block* list_blocks;
};

struct flash_bus {
	/* chip array */
	struct flash_chip* list_chips;
};

struct ssd_info {
	/* ssd information */
	uint32_t nr_buses;
	uint32_t nr_chips_per_bus;
	uint32_t nr_blocks_per_chip;
	uint32_t nr_pages_per_block;

	/* bus array */
	struct flash_bus* list_buses;
};


/* page info interface */
uint32_t get_mapped_lpa (struct flash_page *pgi, uint8_t ofs);
void set_mapped_lpa (struct flash_page *pgi, uint8_t ofs, uint32_t lpa);
enum fb_pg_status_t get_pg_status (struct flash_page *pgi, uint8_t ofs);
void set_pg_status (struct flash_page *pgi, uint8_t ofs, enum fb_pg_status_t status);
uint32_t get_nr_invalid_lps (struct flash_page *pgi);
void set_nr_invalid_lps (struct flash_page *pgi, uint32_t value);
uint32_t inc_nr_invalid_lps (struct flash_page *pgi);
int get_del_flag_pg (struct flash_page *pgi);
void set_del_flag_pg (struct flash_page *pgi, int flag);

/* block info interface */
void init_blk_inf (struct flash_block *blki);
uint32_t get_bus_idx (struct flash_block *blki);
uint32_t get_chip_idx (struct flash_block *blki);
uint32_t get_blk_idx (struct flash_block *blki);
void set_bus_idx (struct flash_block *blki, uint32_t value);
void set_chip_idx (struct flash_block *blki, uint32_t value);
void set_blk_idx (struct flash_block *blki, uint32_t value);
struct flash_page *get_pgi_from_blki (struct flash_block *blki, uint32_t pg);

uint32_t get_nr_valid_pgs (struct flash_block *blki);
uint32_t get_nr_invalid_pgs (struct flash_block *blki);
uint32_t get_nr_free_pgs (struct flash_block *blki);
void set_nr_valid_pgs (struct flash_block *blki, uint32_t value);
void set_nr_invalid_pgs (struct flash_block *blki, uint32_t value);
void set_nr_free_pgs (struct flash_block *blki, uint32_t value);
uint32_t inc_nr_valid_pgs (struct flash_block *blki);
uint32_t inc_nr_invalid_pgs (struct flash_block *blki);
uint32_t inc_nr_free_pgs (struct flash_block *blki);
uint32_t dec_nr_valid_pgs (struct flash_block *blki);
uint32_t dec_nr_invalid_pgs (struct flash_block *blki);
uint32_t dec_nr_free_pgs (struct flash_block *blki);

uint32_t get_nr_valid_lps_in_blk (struct flash_block *blki);
uint32_t get_nr_invalid_lps_in_blk (struct flash_block *blki);
void set_nr_valid_lps_in_blk (struct flash_block *blki, uint32_t value);
void set_nr_invalid_lps_in_blk (struct flash_block *blki, uint32_t value);
uint32_t inc_nr_valid_lps_in_blk (struct flash_block *blk);
uint32_t inc_nr_invalid_lps_in_blk (struct flash_block *blk);
uint32_t dec_nr_valid_lps_in_blk (struct flash_block *blk);
uint32_t dec_nr_invalid_lps_in_blk (struct flash_block *blk);

uint32_t get_bers_cnt (struct flash_block *blki);
void set_bers_cnt (struct flash_block *blki, uint32_t value);
uint32_t inc_bers_cnt (struct flash_block *blki);

int is_bad_blk (struct flash_block *blki);
void set_bad_blk_flag (struct flash_block *blki, int flag);

int is_rsv_blk (struct flash_block *blki);
void set_rsv_blk_flag (struct flash_block *blki, int flag);

int is_act_blk (struct flash_block *blki);
void set_act_blk_flag (struct flash_block *blki, int flag);

int get_del_flag_blk (struct flash_block *blki);
void set_del_flag_blk (struct flash_block *blki, int flag);


/* chip info interface */
void set_free_blk (struct ssd_info *ssdi, struct flash_block *bi);
void reset_free_blk (struct ssd_info *ssdi, struct flash_block *bi);
struct flash_block *get_free_block (struct ssd_info *ssdi, uint32_t bus, uint32_t chip);
uint32_t get_nr_free_blks_in_chip (struct flash_chip *ci);
void set_used_blk (struct ssd_info* ssdi, struct flash_block *bi);
void reset_used_blk (struct ssd_info* ssdi, struct flash_block *bi);
struct flash_block *get_used_block (struct ssd_info *ssdi, uint32_t bus, uint32_t chip);
uint32_t get_nr_used_blks_in_chip (struct flash_chip *ci);
void set_dirt_blk (struct ssd_info* ssdi, struct flash_block *bi);
void reset_dirt_blk (struct ssd_info* ssdi, struct flash_block *bi);
struct flash_block *get_dirt_block (struct ssd_info *ssdi, uint32_t bus, uint32_t chip);
uint32_t get_nr_dirt_blks_in_chip (struct flash_chip *ci);
int is_dirt_blk (struct flash_block *blki);

uint32_t get_nr_free_blks (struct flash_chip *chipi);
void set_nr_free_blks (struct flash_chip *chipi, uint32_t value);
uint32_t inc_nr_free_blks (struct flash_chip *chipi);
uint32_t dec_nr_free_blks (struct flash_chip *chipi);

/* create the ftl information structure */
struct ssd_info* create_ssd_info (void);

/* destory the ftl information structure */
void destroy_ssd_info (struct ssd_info *ptr_ssd_info);

struct flash_bus* get_bus_info(
		struct ssd_info *ptr_ssd_info,
		uint32_t bus);

struct flash_chip* get_chip_info(
		struct ssd_info *ptr_ssd_info,
		uint32_t bus,
		uint32_t chip);

struct flash_block* get_block_info(
		struct ssd_info *ptr_ssd_info,
		uint32_t bus,
		uint32_t chip,
		uint32_t block);

struct flash_page* get_page_info(
		struct ssd_info *ptr_ssd_info,
		uint32_t bus,
		uint32_t chip,
		uint32_t block,
		uint32_t page);

int get_chip_status(
		struct ssd_info *ptr_ssd_info,
		uint8_t bus,
		uint8_t chip);

void set_chip_status(
		struct ssd_info *ptr_ssd_info,
		uint8_t bus,
		uint8_t chip,
		int new_status);
