#pragma once
#include <linux/types.h>

struct fb_context_t;
struct flash_block;

int alloc_new_page(struct fb_context_t *ptr_fb_context, u8 bus, u8 chip,
                   u32 *ptr_block, u32 *ptr_page);
int map_logical_to_physical(struct fb_context_t *ptr_fb_context,
                            u32 *logical_page_address, u32 bus, u32 chip,
                            u32 block, u32 page);
void update_act_blk(struct fb_context_t *fb, u8 bus, u8 chip);
u32 invalidate_lpg(struct fb_context_t *fb, u32 lpa);
void get_prev_bus_chip(struct fb_context_t *ptr_fb_context, u8 *bus, u8 *chip);
