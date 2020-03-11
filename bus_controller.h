struct vdevice_t;
struct fb_bus_controller_t;
struct fb_bio_t;

int fb_bus_controller_init(struct vdevice_t *ptr_vdevice,
                           u32 num_max_entries_per_chip);
void fb_bus_controller_destroy(struct fb_bus_controller_t **ptr_bus_controller);
int fb_issue_operation(struct fb_bus_controller_t *ptr_bus_controller, u32 chip,
                       u32 operation, struct fb_bio_t *ptr_bio);
