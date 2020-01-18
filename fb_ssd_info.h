#define IDLE 0
#define FG_MAPPING 1
#define BG_GC 2

#define blk_list_for_each(head, el) DL_FOREACH(head, el)

typedef enum {
	PAGE_STATUS_FREE,
	PAGE_STATUS_VALID,
	PAGE_STATUS_INVALID,
	PAGE_STATUS_DEL
} fb_pg_status_t;

typedef struct flash_page_t {
	fb_pg_status_t page_status[NR_LP_IN_PP];	
	uint32_t no_logical_page_addr[NR_LP_IN_PP];
	uint32_t nr_invalid_log_pages;

	int del_flag;
} fb_pg_inf_t;

typedef struct flash_block_t {
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
	struct flash_page_t* list_pages;

	struct flash_block_t *prev, *next;

	int del_flag;
} fb_blk_inf_t;

typedef struct flash_chip_t {
	struct completion chip_status_lock;

	int chip_status;
	/* the number of free blocks in a chip */
	uint32_t nr_free_blocks;
	uint32_t nr_used_blks;
	uint32_t nr_dirt_blks;

	fb_blk_inf_t *free_blks;
	fb_blk_inf_t *used_blks;
	fb_blk_inf_t *dirt_blks;

	/* the number of dirty blocks in a chip */
	uint32_t nr_dirty_blocks;

	/* block array */
	struct flash_block_t* list_blocks;

} fb_chip_inf_t;

typedef struct flash_bus_t {
	/* chip array */
	struct flash_chip_t* list_chips;
} fb_bus_inf_t;

typedef struct ssd_info_t {
	/* ssd information */
	uint32_t nr_buses;
	uint32_t nr_chips_per_bus;
	uint32_t nr_blocks_per_chip;
	uint32_t nr_pages_per_block;

	/* bus array */
	struct flash_bus_t* list_buses;
} fb_ssd_inf_t;


/* page info interface */
inline uint32_t get_mapped_lpa (fb_pg_inf_t *pgi, uint8_t ofs);
inline void set_mapped_lpa (fb_pg_inf_t *pgi, uint8_t ofs, uint32_t lpa);
inline fb_pg_status_t get_pg_status (fb_pg_inf_t *pgi, uint8_t ofs);
inline void set_pg_status (fb_pg_inf_t *pgi, uint8_t ofs, fb_pg_status_t status);
inline uint32_t get_nr_invalid_lps (fb_pg_inf_t *pgi);
inline void set_nr_invalid_lps (fb_pg_inf_t *pgi, uint32_t value);
inline uint32_t inc_nr_invalid_lps (fb_pg_inf_t *pgi);
inline int get_del_flag_pg (fb_pg_inf_t *pgi);
inline void set_del_flag_pg (fb_pg_inf_t *pgi, int flag);

/* word line interface */
inline int is_invalid_wl (uint32_t ppa);

/* block info interface */
inline void init_blk_inf (fb_blk_inf_t *blki);
inline uint32_t get_bus_idx (fb_blk_inf_t *blki);
inline uint32_t get_chip_idx (fb_blk_inf_t *blki);
inline uint32_t get_blk_idx (fb_blk_inf_t *blki);
inline void set_bus_idx (fb_blk_inf_t *blki, uint32_t value);
inline void set_chip_idx (fb_blk_inf_t *blki, uint32_t value);
inline void set_blk_idx (fb_blk_inf_t *blki, uint32_t value);
inline fb_pg_inf_t *get_pgi_from_blki (fb_blk_inf_t *blki, uint32_t pg);

inline uint32_t get_nr_valid_pgs (fb_blk_inf_t *blki);
inline uint32_t get_nr_invalid_pgs (fb_blk_inf_t *blki);
inline uint32_t get_nr_free_pgs (fb_blk_inf_t *blki);
inline void set_nr_valid_pgs (fb_blk_inf_t *blki, uint32_t value);
inline void set_nr_invalid_pgs (fb_blk_inf_t *blki, uint32_t value);
inline void set_nr_free_pgs (fb_blk_inf_t *blki, uint32_t value);
inline uint32_t inc_nr_valid_pgs (fb_blk_inf_t *blki);
inline uint32_t inc_nr_invalid_pgs (fb_blk_inf_t *blki);
inline uint32_t inc_nr_free_pgs (fb_blk_inf_t *blki);
inline uint32_t dec_nr_valid_pgs (fb_blk_inf_t *blki);
inline uint32_t dec_nr_invalid_pgs (fb_blk_inf_t *blki);
inline uint32_t dec_nr_free_pgs (fb_blk_inf_t *blki);

inline uint32_t get_nr_valid_lps_in_blk (fb_blk_inf_t *blki);
inline uint32_t get_nr_invalid_lps_in_blk (fb_blk_inf_t *blki);
inline void set_nr_valid_lps_in_blk (fb_blk_inf_t *blki, uint32_t value);
inline void set_nr_invalid_lps_in_blk (fb_blk_inf_t *blki, uint32_t value);
inline uint32_t inc_nr_valid_lps_in_blk (fb_blk_inf_t *blk);
inline uint32_t inc_nr_invalid_lps_in_blk (fb_blk_inf_t *blk);
inline uint32_t dec_nr_valid_lps_in_blk (fb_blk_inf_t *blk);
inline uint32_t dec_nr_invalid_lps_in_blk (fb_blk_inf_t *blk);

inline uint32_t get_bers_cnt (fb_blk_inf_t *blki);
inline void set_bers_cnt (fb_blk_inf_t *blki, uint32_t value);
inline uint32_t inc_bers_cnt (fb_blk_inf_t *blki);

inline int is_bad_blk (fb_blk_inf_t *blki);
inline void set_bad_blk_flag (fb_blk_inf_t *blki, int flag);

inline int is_rsv_blk (fb_blk_inf_t *blki);
inline void set_rsv_blk_flag (fb_blk_inf_t *blki, int flag);

inline int is_act_blk (fb_blk_inf_t *blki);
inline void set_act_blk_flag (fb_blk_inf_t *blki, int flag);

inline int get_del_flag_blk (fb_blk_inf_t *blki);
inline void set_del_flag_blk (fb_blk_inf_t *blki, int flag);


/* chip info interface */
inline void set_free_blk (fb_ssd_inf_t *ssdi, fb_blk_inf_t *bi);
inline void reset_free_blk (fb_ssd_inf_t *ssdi, fb_blk_inf_t *bi);
inline fb_blk_inf_t *get_free_block (fb_ssd_inf_t *ssdi, uint32_t bus, uint32_t chip);
inline uint32_t get_nr_free_blks_in_chip (fb_chip_inf_t *ci);
inline void set_used_blk (fb_ssd_inf_t* ssdi, fb_blk_inf_t *bi);
inline void reset_used_blk (fb_ssd_inf_t* ssdi, fb_blk_inf_t *bi);
inline fb_blk_inf_t *get_used_block (fb_ssd_inf_t *ssdi, uint32_t bus, uint32_t chip);
inline uint32_t get_nr_used_blks_in_chip (fb_chip_inf_t *ci);
inline void set_dirt_blk (fb_ssd_inf_t* ssdi, fb_blk_inf_t *bi);
inline void reset_dirt_blk (fb_ssd_inf_t* ssdi, fb_blk_inf_t *bi);
inline fb_blk_inf_t *get_dirt_block (fb_ssd_inf_t *ssdi, uint32_t bus, uint32_t chip);
inline uint32_t get_nr_dirt_blks_in_chip (fb_chip_inf_t *ci);
inline int is_dirt_blk (fb_blk_inf_t *blki);

inline uint32_t get_nr_free_blks (fb_chip_inf_t *chipi);
inline void set_nr_free_blks (fb_chip_inf_t *chipi, uint32_t value);
inline uint32_t inc_nr_free_blks (fb_chip_inf_t *chipi);
inline uint32_t dec_nr_free_blks (fb_chip_inf_t *chipi);

/* create the ftl information structure */
struct ssd_info_t* create_ssd_info (void);

/* destory the ftl information structure */
void destroy_ssd_info (struct ssd_info_t *ptr_ssd_info);

struct flash_bus_t* get_bus_info(
		struct ssd_info_t *ptr_ssd_info, 
		uint32_t bus);

struct flash_chip_t* get_chip_info(
		struct ssd_info_t *ptr_ssd_info, 
		uint32_t bus, 
		uint32_t chip);

struct flash_block_t* get_block_info(
		struct ssd_info_t *ptr_ssd_info, 
		uint32_t bus,
		uint32_t chip,
		uint32_t block);

struct flash_page_t* get_page_info(
		struct ssd_info_t *ptr_ssd_info, 
		uint32_t bus,
		uint32_t chip,
		uint32_t block,
		uint32_t page);

inline int get_chip_status(
		struct ssd_info_t *ptr_ssd_info,
		uint8_t bus,
		uint8_t chip);

inline void set_chip_status(
		struct ssd_info_t *ptr_ssd_info,
		uint8_t bus,
		uint8_t chip,
		int new_status);
