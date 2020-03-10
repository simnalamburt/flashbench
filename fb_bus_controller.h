#include <linux/completion.h>

struct fb_operation_t
{
	uint32_t operation;

	struct fb_bio_t *ptr_fb_bio;
};

struct fb_opr_queue_t
{
	uint32_t num_max_entries;
	uint32_t num_entries;

	uint32_t queue_head;
	uint32_t queue_tail;

	struct fb_operation_t *opr_list;

	struct completion queue_lock;
};

struct fb_chip_busy_t
{
	uint32_t wakeup_time_in_us;
	uint32_t issue_time_in_us;

	struct completion chip_busy;

	struct fb_bio_t *ptr_fb_bio;
};

struct fb_bus_controller_t
{
	uint32_t num_bus;

	struct fb_opr_queue_t **ptr_opr_queue;

	struct fb_chip_busy_t *chip_busies;

	uint32_t flag_enable_thread;

	struct task_struct *ptr_task;
};

int fb_bus_controller_init(struct vdevice_t *ptr_vdevice, uint32_t num_max_entries_per_chip);
void fb_bus_controller_destroy(struct fb_bus_controller_t **ptr_bus_controller);
int fb_issue_operation(
		struct fb_bus_controller_t *ptr_bus_controller,
		uint32_t chip, uint32_t operation, struct fb_bio_t *ptr_bio);
