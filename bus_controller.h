#include <linux/completion.h>

struct fb_operation_t
{
	u32 operation;

	struct fb_bio_t *ptr_fb_bio;
};

struct fb_opr_queue_t
{
	u32 num_max_entries;
	u32 num_entries;

	u32 queue_head;
	u32 queue_tail;

	struct fb_operation_t *opr_list;

	struct completion queue_lock;
};

struct fb_chip_busy_t
{
	u32 wakeup_time_in_us;
	u32 issue_time_in_us;

	struct completion chip_busy;

	struct fb_bio_t *ptr_fb_bio;
};

struct fb_bus_controller_t
{
	u32 num_bus;

	struct fb_opr_queue_t **ptr_opr_queue;

	struct fb_chip_busy_t *chip_busies;

	u32 flag_enable_thread;

	struct task_struct *ptr_task;
};

int fb_bus_controller_init(struct vdevice_t *ptr_vdevice, u32 num_max_entries_per_chip);
void fb_bus_controller_destroy(struct fb_bus_controller_t **ptr_bus_controller);
int fb_issue_operation(
		struct fb_bus_controller_t *ptr_bus_controller,
		u32 chip, u32 operation, struct fb_bio_t *ptr_bio);
