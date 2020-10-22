#pragma once
#include <linux/types.h>
#include "rust/libflashbench.h"

struct vdevice_t;
struct fb_bus_controller_t;
struct fb_bio_t;

int fb_bus_controller_init(struct vdevice_t *ptr_vdevice,
                           u32 num_max_entries_per_chip);
void fb_bus_controller_destroy(struct fb_bus_controller_t **ptr_bus_controller);
void fb_issue_operation(struct fb_bus_controller_t *ptr_bus_controller, u32 chip,
                        enum fb_dev_op_t operation, struct fb_bio_t *ptr_bio);
