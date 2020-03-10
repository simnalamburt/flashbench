#include "fb_option.h"

typedef enum {
	OP_READ,
	OP_PROG,
	OP_PLOCK,
	OP_BLOCK,
	OP_BERS,
} fb_dev_op_t;

typedef enum {
	LSB = 0,
	CSB = 1,
	MSB = 2,
} fb_pg_type_t;

fb_pg_type_t page_type (uint32_t pg_idx);

struct vdevice_page_t
{
	uint8_t* ptr_data;
};

struct vdevice_block_t
{
	struct vdevice_page_t pages[NUM_PAGES_PER_BLOCK];
};

struct vdevice_chip_t
{
	struct vdevice_block_t blocks[NUM_BLOCKS_PER_CHIP];
};

struct vdevice_bus_t
{
	struct vdevice_chip_t chips[NUM_CHIPS_PER_BUS];
};

typedef struct vdevice_t {
	uint64_t device_capacity;
	uint64_t logical_capacity;

	uint8_t *ptr_vdisk[NUM_BUSES];
	struct vdevice_bus_t buses[NUM_BUSES];

#if (VDEVICE_TIME_MODELED==TRUE)
	struct fb_bus_controller_t **ptr_bus_controller;
#endif
} fb_vdev_t;

struct vdevice_t *create_vdevice(void);
void destroy_vdevice(struct vdevice_t *ptr_vdevice);

void vdevice_read(
		struct vdevice_t *ptr_vdevice,
		uint8_t bus,
		uint8_t chip,
		uint32_t block,
		uint32_t page,
		uint8_t *page_bitmap,
		uint8_t *ptr_dest,
		fb_bio_t *ptr_fb_bio);

void vdevice_write(
		struct vdevice_t *ptr_vdevice,
		uint8_t bus,
		uint8_t chip,
		uint32_t block,
		uint32_t page,
		const uint8_t *ptr_src,
		fb_bio_t *ptr_fb_bio);

void vdevice_erase(
		struct vdevice_t *ptr_vdevice,
		uint8_t bus,
		uint8_t chip,
		uint32_t block,
		fb_bio_t *ptr_fb_bio);

void vdevice_plock (
		struct vdevice_t *ptr_vdevice,
		uint8_t bus,
		uint8_t chip);


void vdevice_block (
		struct vdevice_t *ptr_vdevice,
		uint8_t bus,
		uint8_t chip);


int is_valid_address_range(uint32_t logical_page_address);

uint32_t convert_to_physical_address(
		uint32_t bus, uint32_t chip, uint32_t block, uint32_t page);

uint32_t convert_to_wl_idx (
		uint32_t bus, uint32_t chip, uint32_t block, uint32_t pg_idx);

void convert_to_ssd_layout(uint32_t logical_page_address,
		uint32_t *ptr_bus, uint32_t *ptr_chip, uint32_t *ptr_block, uint32_t *ptr_page);

#if (VDEVICE_TIME_MODELED==TRUE)
uint32_t operation_time (fb_dev_op_t op);
#endif
