#pragma once
#include <linux/types.h>

struct fb_bio_t;
struct fb_bus_controller_t;
struct fb_gc_mngr_t;
struct fb_context_t;
struct flash_block;

//
// gc_page_mapping
//
struct fb_gc_mngr_t *create_gc_mngr(struct fb_context_t *fb);
void destroy_gc_mngr(struct fb_gc_mngr_t *gcm);
s32 trigger_gc_page_mapping(struct fb_context_t *ptr_fb_context);
void init_gcm(struct fb_gc_mngr_t *gcm);
s32 update_gc_blks(struct fb_context_t *fb);
s32 fb_bgc_prepare_act_blks(struct fb_context_t *fb);
s32 fb_bgc_set_vic_blks(struct fb_context_t *fb);
s32 fb_bgc_read_valid_pgs(struct fb_context_t *fb);
s32 prog_valid_pgs_to_gc_blks(struct fb_context_t *fb);

//
// page_mapping_function
//
s32 alloc_new_page(struct fb_context_t *ptr_fb_context, u8 bus, u8 chip,
                   u32 *ptr_block, u32 *ptr_page);
s32 map_logical_to_physical(struct fb_context_t *ptr_fb_context,
                            u32 *logical_page_address, u32 bus, u32 chip,
                            u32 block, u32 page);
void update_act_blk(struct fb_context_t *fb, u8 bus, u8 chip);
u32 invalidate_lpg(struct fb_context_t *fb, u32 lpa);
void get_prev_bus_chip(struct fb_context_t *ptr_fb_context, u8 *bus, u8 *chip);

//
// vdevice
//
enum fb_dev_op_t { OP_READ, OP_PROG, OP_PLOCK, OP_BLOCK, OP_BERS };
enum {
  // Hardware configuration
  NUM_BUSES = 1,
  NUM_CHIPS_PER_BUS = 2,
  NUM_BLOCKS_PER_CHIP = 171,
  NUM_PAGES_PER_BLOCK = 16 * 4 * 3,  // v-layer * h-layer * multi-level degree
};

struct vdevice_page_t {
  u8 *ptr_data;
};
struct vdevice_block_t {
  struct vdevice_page_t pages[NUM_PAGES_PER_BLOCK];
};
struct vdevice_chip_t {
  struct vdevice_block_t blocks[NUM_BLOCKS_PER_CHIP];
};
struct vdevice_bus_t {
  struct vdevice_chip_t chips[NUM_CHIPS_PER_BUS];
};
struct vdevice_t {
  u64 device_capacity;
  u64 logical_capacity;
  u8 *ptr_vdisk[NUM_BUSES];
  struct vdevice_bus_t buses[NUM_BUSES];
  struct fb_bus_controller_t **ptr_bus_controller;
};

struct vdevice_t *create_vdevice(void);
void destroy_vdevice(struct vdevice_t *ptr_vdevice);
void vdevice_read(struct vdevice_t *ptr_vdevice, u8 bus, u8 chip, u32 block,
                  u32 page, u8 *page_bitmap, u8 *ptr_dest,
                  struct fb_bio_t *ptr_fb_bio);
void vdevice_write(struct vdevice_t *ptr_vdevice, u8 bus, u8 chip, u32 block,
                   u32 page, const u8 *ptr_src, struct fb_bio_t *ptr_fb_bio);
void vdevice_erase(struct vdevice_t *ptr_vdevice, u8 bus, u8 chip, u32 block,
                   struct fb_bio_t *ptr_fb_bio);
s32 is_valid_address_range(u32 logical_page_address);
u32 convert_to_physical_address(u32 bus, u32 chip, u32 block, u32 page);
void convert_to_ssd_layout(u32 logical_page_address, u32 *ptr_bus,
                           u32 *ptr_chip, u32 *ptr_block, u32 *ptr_page);
u32 operation_time(enum fb_dev_op_t op);
