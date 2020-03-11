struct fb_context_t;
struct flash_block;

u32 get_mapped_physical_address(struct fb_context_t *ptr_fb_context,
                                u32 logical_page_address, u32 *ptr_bus,
                                u32 *ptr_chip, u32 *ptr_block, u32 *ptr_page);

int alloc_new_page(struct fb_context_t *ptr_fb_context, u8 bus, u8 chip,
                   u32 *ptr_block, u32 *ptr_page);

int map_logical_to_physical(struct fb_context_t *ptr_fb_context,
                            u32 *logical_page_address, u32 bus, u32 chip,
                            u32 block, u32 page);

void update_act_blk(struct fb_context_t *fb, u8 bus, u8 chip);

u32 invalidate_lpg(struct fb_context_t *fb, u32 lpa);

struct flash_block *get_curr_gc_block(struct fb_context_t *ptr_fb_context,
                                      u32 bus, u32 chip);

struct flash_block *get_curr_active_block(struct fb_context_t *ptr_fb_context,
                                          u32 bus, u32 chip);

void set_curr_gc_block(struct fb_context_t *ptr_fb_context, u32 bus, u32 chip,
                       struct flash_block *ptr_new_block);

void set_curr_active_block(struct fb_context_t *ptr_fb_context, u32 bus,
                           u32 chip, struct flash_block *ptr_new_block);

void set_prev_bus_chip(struct fb_context_t *ptr_fb_context, u8 bus, u8 chip);

void get_prev_bus_chip(struct fb_context_t *ptr_fb_context, u8 *bus, u8 *chip);

void get_next_bus_chip(struct fb_context_t *ptr_fb_context, u8 *ptr_bus,
                       u8 *ptr_chip);
