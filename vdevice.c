#include <linux/vmalloc.h>

#include "main.h"
#include "option.h"
#include "vdevice.h"
#if (VDEVICE_TIME_MODELED == TRUE)
#include "bus_controller.h"
#endif

#if (VDEVICE_TYPE == RAM_DISK)
struct vdevice_t *create_vdevice(void) {
  struct vdevice_t *ptr_vdevice = NULL;
  u64 bus_loop, chip_loop, block_loop, page_loop;
  u64 bus_capacity;

  if ((ptr_vdevice = (struct vdevice_t *)vmalloc(sizeof(struct vdevice_t))) ==
      NULL) {
    printk(KERN_ERR
           "[FlashBench] Allocating virtual device structure failed.\n");
    goto FAIL_ALLOC_VDEVICE;
  }

  bus_capacity = NUM_CHIPS_PER_BUS;
  bus_capacity *= NUM_BLOCKS_PER_CHIP;
  bus_capacity *= NUM_PAGES_PER_BLOCK;
  bus_capacity *= PHYSICAL_PAGE_SIZE;

  ptr_vdevice->device_capacity = NUM_BUSES * bus_capacity;

  for (bus_loop = 0; bus_loop < NUM_BUSES; bus_loop++) {
    if ((ptr_vdevice->ptr_vdisk[bus_loop] =
             (u8 *)vmalloc(sizeof(u8) * bus_capacity)) == NULL) {
      printk(KERN_ERR "[FlashBench] Allocating virtual disk failed.\n");
      goto FAIL_ALLOC_VDISK;
    }
  }

  bus_capacity = NUM_CHIPS_PER_BUS;
  bus_capacity *= NUM_BLOCKS_PER_CHIP;
  bus_capacity *= NUM_PAGES_PER_BLOCK;
  bus_capacity = bus_capacity * CFACTOR_PERCENT / 100;
  bus_capacity *= PHYSICAL_PAGE_SIZE;

  ptr_vdevice->logical_capacity = NUM_BUSES * bus_capacity;

  for (bus_loop = 0; bus_loop < NUM_BUSES; bus_loop++) {
    for (chip_loop = 0; chip_loop < NUM_CHIPS_PER_BUS; chip_loop++) {
      for (block_loop = 0; block_loop < NUM_BLOCKS_PER_CHIP; block_loop++) {
        for (page_loop = 0; page_loop < NUM_PAGES_PER_BLOCK; page_loop++) {
          ptr_vdevice->buses[bus_loop]
              .chips[chip_loop]
              .blocks[block_loop]
              .pages[page_loop]
              .ptr_data =
              ptr_vdevice->ptr_vdisk[bus_loop] +
              (chip_loop * NUM_BLOCKS_PER_CHIP * NUM_PAGES_PER_BLOCK +
               block_loop * NUM_PAGES_PER_BLOCK + page_loop) *
                  PHYSICAL_PAGE_SIZE;
        }
      }
    }
  }

#if (VDEVICE_TIME_MODELED == TRUE)
  if (fb_bus_controller_init(ptr_vdevice, NUM_MAX_ENTRIES_OPR_QUEUE) == -1) {
    goto FAIL_ALLOC_VDISK;
  }
#endif

  return ptr_vdevice;

FAIL_ALLOC_VDISK:
  if (ptr_vdevice != NULL) {
    vfree(ptr_vdevice);
  }

FAIL_ALLOC_VDEVICE:
  return NULL;
}

void destroy_vdevice(struct vdevice_t *ptr_vdevice) {
  u32 loop_bus;
  if (ptr_vdevice != NULL) {
    fb_bus_controller_destroy(ptr_vdevice->ptr_bus_controller);

    for (loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
      if (ptr_vdevice->ptr_vdisk[loop_bus] != NULL) {
        vfree(ptr_vdevice->ptr_vdisk[loop_bus]);
      }
    }

    vfree(ptr_vdevice);
  }
}

void vdevice_read(struct vdevice_t *ptr_vdevice, u8 bus, u8 chip, u32 block,
                  u32 page, u8 *page_bitmap, u8 *ptr_dest,
                  struct fb_bio_t *ptr_fb_bio) {
  u8 *ptr_src =
      ptr_vdevice->buses[bus].chips[chip].blocks[block].pages[page].ptr_data;
  u8 lp_loop;
  u8 *ptr_curr = ptr_dest;
  for (lp_loop = 0; lp_loop < NR_LP_IN_PP; lp_loop++) {
    if (page_bitmap[lp_loop] == 1) {
      memcpy(ptr_curr, ptr_src, LOGICAL_PAGE_SIZE);
      ptr_curr += LOGICAL_PAGE_SIZE;
    }
    ptr_src += LOGICAL_PAGE_SIZE;
  }

#if (VDEVICE_TIME_MODELED == TRUE)
  fb_issue_operation(ptr_vdevice->ptr_bus_controller[bus], chip, OP_READ,
                     ptr_fb_bio);
#endif
}

void vdevice_write(struct vdevice_t *ptr_vdevice, u8 bus, u8 chip, u32 block,
                   u32 page, const u8 *ptr_src, struct fb_bio_t *ptr_fb_bio) {
  u8 *ptr_dest =
      ptr_vdevice->buses[bus].chips[chip].blocks[block].pages[page].ptr_data;

  memcpy(ptr_dest, ptr_src, PHYSICAL_PAGE_SIZE);

#if (VDEVICE_TIME_MODELED == TRUE)
  fb_issue_operation(ptr_vdevice->ptr_bus_controller[bus], chip, OP_PROG,
                     ptr_fb_bio);
#endif
}

void vdevice_erase(struct vdevice_t *ptr_vdevice, u8 bus, u8 chip, u32 block,
                   struct fb_bio_t *ptr_fb_bio) {
  u8 *ptr_dest =
      ptr_vdevice->buses[bus].chips[chip].blocks[block].pages[0].ptr_data;

  memset(ptr_dest, 0xFF, NUM_PAGES_PER_BLOCK * PHYSICAL_PAGE_SIZE);
#if (VDEVICE_TIME_MODELED == TRUE)
  fb_issue_operation(ptr_vdevice->ptr_bus_controller[bus], chip, OP_BERS,
                     ptr_fb_bio);
#endif
}
#endif

int is_valid_address_range(u32 logical_page_address) {
  return logical_page_address < NUM_LOG_PAGES;
}

u32 convert_to_physical_address(u32 bus, u32 chip, u32 block, u32 page) {
  // return (bus << 27) | (chip << 21) | (block << 8) | page;
  // Max buses: 16 - 4
  // Max chips: 16 - 4
  // Max blocks: 4096 - 12
  // Max pages: 4096 - 12
  return (bus << 28) | (chip << 24) | (block << 12) | page;
}

void convert_to_ssd_layout(u32 logical_page_address, u32 *ptr_bus,
                           u32 *ptr_chip, u32 *ptr_block, u32 *ptr_page) {
  *ptr_bus = (0xF & logical_page_address >> 28);
  *ptr_chip = (0xF & logical_page_address >> 24);
  *ptr_block = (0xFFF & logical_page_address >> 12);
  *ptr_page = (0xFFF & logical_page_address);
}

#if (VDEVICE_TIME_MODELED == TRUE)
u32 operation_time(enum fb_dev_op_t op) {
  switch (op) {
    case OP_READ:
      return TREAD;
    case OP_PROG:
      return TPROG;
    case OP_PLOCK:
      return TPLOCK;
    case OP_BLOCK:
      return TBLOCK;
    case OP_BERS:
      return TBERS;
    default:
      return 0;
  }
}
#endif
