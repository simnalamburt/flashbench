
uint32_t get_mapped_physical_address(
		struct fb_context_t *ptr_fb_context,
		uint32_t logical_page_address,
		uint32_t *ptr_bus,
		uint32_t *ptr_chip,
		uint32_t *ptr_block,
		uint32_t *ptr_page);

int alloc_new_page(
		struct fb_context_t *ptr_fb_context,
		uint8_t bus,
		uint8_t chip,
		uint32_t *ptr_block,
		uint32_t *ptr_page);

int map_logical_to_physical(
		struct fb_context_t *ptr_fb_context,
		uint32_t *logical_page_address,
		uint32_t bus,
		uint32_t chip,
		uint32_t block,
		uint32_t page);

void update_act_blk (fb_t* fb, uint8_t bus, uint8_t chip);

uint32_t invalidate_lpg (fb_t *fb, uint32_t lpa);

inline struct flash_block_t* get_curr_gc_block(
		struct fb_context_t *ptr_fb_context,
		uint32_t bus, uint32_t chip);

inline struct flash_block_t* get_curr_active_block(
		struct fb_context_t *ptr_fb_context,
		uint32_t bus, uint32_t chip);

inline void set_curr_gc_block(
		struct fb_context_t *ptr_fb_context,
		uint32_t bus, uint32_t chip, struct flash_block_t* ptr_new_block);

inline void set_curr_active_block(
		struct fb_context_t *ptr_fb_context,
		uint32_t bus, uint32_t chip, struct flash_block_t* ptr_new_block);

inline void set_prev_bus_chip(
		struct fb_context_t *ptr_fb_context,
		uint8_t bus,
		uint8_t chip);

inline void get_prev_bus_chip (
		struct fb_context_t *ptr_fb_context,
		uint8_t *bus, uint8_t *chip);

void get_next_bus_chip(
		struct fb_context_t *ptr_fb_context,
		uint8_t *ptr_bus,
		uint8_t *ptr_chip);

