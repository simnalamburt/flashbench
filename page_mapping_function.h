#pragma once
#include <linux/types.h>
struct fb_context_t;
struct page_mapping_context_t;

u32 get_mapped_physical_address(struct fb_context_t *ptr_fb_context,
                                u32 logical_page_address, u32 *ptr_bus,
                                u32 *ptr_chip, u32 *ptr_block, u32 *ptr_page);
void set_prev_bus_chip(struct fb_context_t *ptr_fb_context, u8 bus, u8 chip);
void get_next_bus_chip(struct fb_context_t *ptr_fb_context, u8 *ptr_bus,
                       u8 *ptr_chip);
u32 get_mapped_ppa(struct page_mapping_context_t *ftl, u32 lpa);
void set_mapped_ppa(struct page_mapping_context_t *ftl, u32 lpa, u32 ppa);
struct flash_block *get_curr_gc_block(struct fb_context_t *ptr_fb_context,
                                      u32 bus, u32 chip);
void set_curr_gc_block(struct fb_context_t *ptr_fb_context, u32 bus, u32 chip,
                       struct flash_block *ptr_new_block);
struct flash_block *get_curr_active_block(struct fb_context_t *ptr_fb_context,
                                          u32 bus, u32 chip);
void set_curr_active_block(struct fb_context_t *ptr_fb_context, u32 bus,
                           u32 chip, struct flash_block *ptr_new_block);
