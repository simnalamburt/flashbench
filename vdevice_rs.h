#pragma once
#include <linux/types.h>
#include "option.h"

struct fb_bio_t;
struct fb_bus_controller_t;

enum fb_dev_op_t {
  OP_READ,
  OP_PROG,
  OP_PLOCK,
  OP_BLOCK,
  OP_BERS,
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

int is_valid_address_range(u32 logical_page_address);

u32 convert_to_physical_address(u32 bus, u32 chip, u32 block, u32 page);

void convert_to_ssd_layout(u32 logical_page_address, u32 *ptr_bus,
                           u32 *ptr_chip, u32 *ptr_block, u32 *ptr_page);

u32 operation_time(enum fb_dev_op_t op);
