#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/vmalloc.h>


#include "fb.h"
#include "fb_option.h"
#include "fb_util.h"
#include "fb_vdevice.h"
#include "fb_bus_controller.h"

#if (LOG_TIMING==TRUE)
static u32 nr_timer_log_delay_records[NUM_BUSES] = {0, };
static u32 max_timer_log_delay_records = 1000000;
static u32 timer_log_delay_records[NUM_BUSES][1000000];

static u32 nr_timer_log_resp_records[NUM_BUSES] = {0, };
static u32 max_timer_log_resp_records = 1000000;
static u32 timer_log_resp_records[NUM_BUSES][1000000];
#endif

// ---------------- prototypes of static functions: bus controller ----------------------------
// Creating an bus controller for a bus
static struct fb_bus_controller_t *create_bus_controller(u32 num_max_entries_per_chip);

// Destroying the bus controller
static void destroy_bus_controller(struct fb_bus_controller_t *ptr_bus_controller);

// Bus controllers management thread
// Phase 1: Release bus locks whose conditions are satisfied
// Phase 2: Acquire bus locks for requests waiting for avaliable chips
static int fb_bus_ctrl_thread(void *arg);
static int fb_init_bus_ctrl_thread(struct fb_bus_controller_t *ptr_bus_controller, u32 bus);
void fb_stop_bus_ctrl_thread(struct fb_bus_controller_t *ptr_bus_controller);
// ---------------- prototypes of static functions: operation queue ----------------------------

// Creating an operation queue for a chip
static struct fb_opr_queue_t *create_opr_queue(u32 num_max_entries);

// Destroying the operation queue
static void destroy_opr_queue(struct fb_opr_queue_t *ptr_opr_queue);

// Get the current number of entries in the target operation queue
static inline u32 opr_queue_num_entries(struct fb_opr_queue_t *ptr_opr_queue);

// Check whether the operation queue is full or not(full: TRUE, otherwise: FASLE)
static inline int opr_queue_full(struct fb_opr_queue_t *ptr_opr_queue);

// Check whether the operation queue is empty or not(empty: TRUE, otherwise: FALSE)
static inline int opr_queue_empty(struct fb_opr_queue_t *ptr_opr_queue);

// Put an entry into the target operation queue(success: 0, error: -1)
static int opr_queue_put_entry(
		struct fb_opr_queue_t *ptr_opr_queue,
		u32 operation,
		struct fb_bio_t *ptr_fb_bio);

// Get informations of the first entry in the target operation queue(success: 0, error: -1)
static int opr_queue_get_first(
		struct fb_opr_queue_t *ptr_opr_queue,
		u32 *ptr_operation,
		struct fb_bio_t **ptr_fb_bio);

// Remove informations of the first entry in the target operation queue(success: 0, error: -1)
static int opr_queue_remove_first(
		struct fb_opr_queue_t *ptr_opr_queue);

static void release_busy_lock(
		struct fb_bus_controller_t *ptr_bus_controller,
		u32 chip);

static void acquire_busy_lock(
		struct fb_bus_controller_t *ptr_bus_controller,
		u32 chip,
		u32 operation,
		struct fb_bio_t *ptr_fb_bio);

static int chip_status_busy(
		struct fb_bus_controller_t *ptr_bus_controller, u32 chip);

static inline u32 get_chip_wakeup_time(
		struct fb_bus_controller_t *ptr_bus_controller, u32 chip);

static inline u32 get_chip_issue_time(
		struct fb_bus_controller_t *ptr_bus_controller, u32 chip);
// ----------------- Public functions ----------------------------------------
// Creating and initialize bus controllers in the virtual device structure
int fb_bus_controller_init(struct vdevice_t *ptr_vdevice, u32 num_max_entries_per_chip)
{
	u32 loop_bus;

	struct fb_bus_controller_t **ptr_bus_controller = NULL;

	// Allocating a pointer of the bus controller list
	if((ptr_bus_controller =
				(struct fb_bus_controller_t **) kmalloc(sizeof(struct fb_bus_controller_t *) * NUM_BUSES, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Allocating the pointer of bus controllers failed.\n");
		goto ALLOC_PTR_BUS_CTRL_FAIL;
	}

	// Creating bus controllers(for the number of buses)
	for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
	{
		if((ptr_bus_controller[loop_bus] = create_bus_controller(num_max_entries_per_chip)) == NULL)
		{
			printk(KERN_ERR "[FlashBench] bus controller: Creating bus controllers failed.\n");
			goto CREATE_BUS_CTRL_FAIL;
		}
	}

	// Initilaize bus control threads(for the number of buses)
	for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
	{
		ptr_bus_controller[loop_bus]->flag_enable_thread = 1;
		ptr_bus_controller[loop_bus]->num_bus = loop_bus;

		if(fb_init_bus_ctrl_thread(ptr_bus_controller[loop_bus], loop_bus) == -1)
		{
			goto INIT_BUS_CTRL_THREAD_FAIL;
		}
	}

	ptr_vdevice->ptr_bus_controller = ptr_bus_controller;

	return 0;

INIT_BUS_CTRL_THREAD_FAIL:
	for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
	{
		ptr_bus_controller[loop_bus]->flag_enable_thread = 0;
		fb_stop_bus_ctrl_thread(ptr_bus_controller[loop_bus]);
	}

CREATE_BUS_CTRL_FAIL:
	if(ptr_bus_controller != NULL)
	{
		for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
		{
			if(ptr_bus_controller[loop_bus] != NULL)
			{
				destroy_bus_controller(ptr_bus_controller[loop_bus]);
			}
			else
			{
				break;
			}
		}

		kfree(ptr_bus_controller);
	}
ALLOC_PTR_BUS_CTRL_FAIL:
	return -1;
}

// Destory bus controllers
void fb_bus_controller_destroy(struct fb_bus_controller_t **ptr_bus_controller)
{
	u32 loop_bus;

	if(ptr_bus_controller != NULL)
	{
		for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
		{
			if(ptr_bus_controller[loop_bus] != NULL)
			{
				ptr_bus_controller[loop_bus]->flag_enable_thread = 0;
				fb_stop_bus_ctrl_thread(ptr_bus_controller[loop_bus]);
			}
		}

		for(loop_bus = 0 ; loop_bus < NUM_BUSES ; loop_bus++)
		{
			if(ptr_bus_controller[loop_bus] != NULL)
			{
				destroy_bus_controller(ptr_bus_controller[loop_bus]);
			}
		}

		kfree(ptr_bus_controller);
	}


}

// Issue an operation for the target chip of the target bus
int fb_issue_operation(
		struct fb_bus_controller_t *ptr_bus_controller,
		u32 chip, u32 operation, struct fb_bio_t *ptr_bio)
{
	while(opr_queue_put_entry(ptr_bus_controller->ptr_opr_queue[chip], operation, ptr_bio) == -1);

	return 0;
}

// ---------------- Static functions: Bus contorller ----------------------------
// Creating an bus controller for a bus
static struct fb_bus_controller_t *create_bus_controller(u32 num_max_entries_per_chip)
{
	struct fb_bus_controller_t *ptr_bus_controller;
	u32 loop_chip;

	// Allocating a pointer of bus contorller structure
	if((ptr_bus_controller =
				(struct fb_bus_controller_t *) kmalloc(sizeof(struct fb_bus_controller_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Allocating bus controller failed.\n");
		goto ALLOC_BUS_CTRL_FAIL;
	}

	// Allocating a pointer of operation queue list(for number of chips)
	if((ptr_bus_controller->ptr_opr_queue =
				(struct fb_opr_queue_t **) kmalloc(sizeof(struct fb_opr_queue_t *) * NUM_CHIPS_PER_BUS, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Allocating pointer of operation queues failed.\n");
		goto ALLOC_PTR_OPR_QUEUE_FAIL;
	}

	// Creating operation queues
	for(loop_chip  = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
	{
		if((ptr_bus_controller->ptr_opr_queue[loop_chip] =
					create_opr_queue(num_max_entries_per_chip)) == NULL)
		{
			printk(KERN_ERR "[FlashBench] bus controller: Creating operation queues failed.\n");
			goto CREATE_OPR_QUEUE_FAIL;
		}
	}

	// Creating chip busy structures
	if((ptr_bus_controller->chip_busies =
				(struct fb_chip_busy_t *) kmalloc(sizeof(struct fb_chip_busy_t) * NUM_CHIPS_PER_BUS, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Allocating chip busy array failed.\n");
		goto CREATE_OPR_QUEUE_FAIL;
	}

	// Initialize chip busy structrues
	for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
	{
		init_completion(&ptr_bus_controller->chip_busies[loop_chip].chip_busy);
		complete(&ptr_bus_controller->chip_busies[loop_chip].chip_busy);

		ptr_bus_controller->chip_busies[loop_chip].wakeup_time_in_us = 0;
		ptr_bus_controller->chip_busies[loop_chip].issue_time_in_us = 0;
		ptr_bus_controller->chip_busies[loop_chip].ptr_fb_bio = NULL;
	}

	// end of the function
	return  ptr_bus_controller;

CREATE_OPR_QUEUE_FAIL:
	if(ptr_bus_controller->ptr_opr_queue != NULL)
	{
		for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
		{
			if(ptr_bus_controller->ptr_opr_queue[loop_chip] != NULL)
			{
				destroy_opr_queue(ptr_bus_controller->ptr_opr_queue[loop_chip]);
			}
			else
			{
				break;
			}
		}

		kfree(ptr_bus_controller->ptr_opr_queue);
	}

ALLOC_PTR_OPR_QUEUE_FAIL:
	if(ptr_bus_controller != NULL)
	{
		kfree(ptr_bus_controller);
	}

ALLOC_BUS_CTRL_FAIL:
	return NULL;
}

// Destroying the bus controller
static void destroy_bus_controller(struct fb_bus_controller_t *ptr_bus_controller)
{
	u32 loop_chip;

	if(ptr_bus_controller != NULL)
	{

		if(ptr_bus_controller->chip_busies != NULL)
		{
			kfree(ptr_bus_controller->chip_busies);
		}

		if(ptr_bus_controller->ptr_opr_queue != NULL)
		{
			for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
			{
				if(ptr_bus_controller->ptr_opr_queue[loop_chip] != NULL)
				{
					destroy_opr_queue(ptr_bus_controller->ptr_opr_queue[loop_chip]);
				}
				else
				{
					break;
				}
			}

			kfree(ptr_bus_controller->ptr_opr_queue);
		}

		kfree(ptr_bus_controller);
	}
}

// Bus controllers management thread
// Phase 1: Release bus locks whose conditions are satisfied
// Phase 2: Acquire bus locks for requests waiting for avaliable chips
static int fb_bus_ctrl_thread(void *arg)
{
	struct fb_bus_controller_t *ptr_bus_controller =
		(struct fb_bus_controller_t *) arg;
	u32 loop_chip;
	u32 wakeup_time_in_us, current_time_in_us;
	u32 operation;
	struct fb_bio_t *ptr_fb_bio = NULL;

	u32 signr;
	int ret = 0;

	allow_signal(SIGKILL);
	allow_signal(SIGSTOP);
	allow_signal(SIGCONT);

	ptr_bus_controller->ptr_task = current;

	set_user_nice(current, -19);
	set_freezable();

	while((ptr_bus_controller->flag_enable_thread) && !kthread_should_stop())
	{
		signr = 0;
		ret = 0;

		allow_signal(SIGHUP);

AGAIN:
		while(signal_pending(current))
		{
			if(try_to_freeze())
			{
				goto AGAIN;
			}

			signr = kernel_dequeue_signal(NULL);

			switch(signr)
			{
				case SIGSTOP:
					set_current_state(TASK_STOPPED);
					break;
				case SIGKILL:
					goto FINISH;
				default:
					break;
			}
			break;
		}

		disallow_signal(SIGHUP);

		// Phase 1: Release bus locks whose conditions are satisfied

		// Get current time
		current_time_in_us = timer_get_timestamp_in_us();

		// Release bus locks whose conditions are satisfied
		for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
		{
			// Get wakeup time, and check it is valid value.
			if((wakeup_time_in_us = get_chip_wakeup_time(ptr_bus_controller, loop_chip)) > 0)
			{
				// Check the condition
				if(wakeup_time_in_us <= current_time_in_us)
				{
					// Release lock
					// If ptr_fb_bio for the chip is not null, the request count decreases.
					// And if the request becomes zero, bio will be returned.
#if (LOG_TIMING==TRUE)
					if(nr_timer_log_delay_records[bus] < max_timer_log_delay_records)
					{
						timer_log_delay_records[bus][nr_timer_log_delay_records[bus]] =
							current_time_in_us - wakeup_time_in_us;
						nr_timer_log_delay_records[bus]++;
					}

					if(nr_timer_log_resp_records[bus] < max_timer_log_resp_records)
					{
						timer_log_resp_records[bus][nr_timer_log_resp_records[bus]] =
							current_time_in_us - get_chip_issue_time(ptr_bus_controller, loop_chip);
						nr_timer_log_resp_records[bus]++;
					}
#endif
					release_busy_lock(ptr_bus_controller, loop_chip);
				}
			}
		}

		// Phase 2: Acquire bus locks for requests waiting for avaliable chips
		for(loop_chip = 0 ; loop_chip < NUM_CHIPS_PER_BUS ; loop_chip++)
		{
			// Check whether the operation queue for the chip is empty or not,
			// and the chip is available or not.
			if(opr_queue_empty(ptr_bus_controller->ptr_opr_queue[loop_chip]) == FALSE &&
					chip_status_busy(ptr_bus_controller, loop_chip) == FALSE)
			{
				// Get informations of the first request for the chip
				if(opr_queue_get_first(ptr_bus_controller->ptr_opr_queue[loop_chip], &operation, &ptr_fb_bio) == -1)
				{
					printk(KERN_ERR "[FlashBench] bus controller: Operation queue should not be empty.\n");
					goto FINISH;
				}

				// Issue the request
				acquire_busy_lock(ptr_bus_controller, loop_chip, operation, ptr_fb_bio);

				// Remove the entry
				opr_queue_remove_first(ptr_bus_controller->ptr_opr_queue[loop_chip]);
			}
		}

		if(ptr_bus_controller->flag_enable_thread == 0)
		{
			yield();
			goto FINISH;
		}

		yield();
	}

FINISH:
	printk(KERN_INFO "[FlashBench] bus controller: End condition of thread\n");
	ptr_bus_controller->flag_enable_thread = 0;
	ptr_bus_controller->ptr_task = NULL;

	return 0;
}

static int fb_init_bus_ctrl_thread(struct fb_bus_controller_t *ptr_bus_controller, u32 bus)
{
	int rc;
	int ret = 0;

	char thread_name[16];
	sprintf(thread_name, "fb_bus_%d", bus);

	ptr_bus_controller->ptr_task = kthread_run(fb_bus_ctrl_thread, (void *) ptr_bus_controller, thread_name);
	rc = IS_ERR(ptr_bus_controller->ptr_task);

	if(rc < 0)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Creating bus %d control task failed.\n", bus);
		ret = -1;
	}
	else
	{
		printk(KERN_INFO "[FlashBench] bus controller: Bus %d controller is successfully created.\n", bus);
	}

	return ret;
}

void fb_stop_bus_ctrl_thread(struct fb_bus_controller_t *ptr_bus_controller)
{
#if (LOG_TIMING==TRUE)
	int i;
	u32 bus = ptr_bus_controller->num_bus;

	char dest[512];
	char src[128];

	// TODO: Change this path
	sprintf(dest, "/home/mult/flashBench/src/log/log_delay_%d.dat", bus);

	for(i = 0 ; i < nr_timer_log_delay_records[bus] ; i++)
	{
		sprintf(src, "%d\n", timer_log_delay_records[bus][i]);
		fb_file_log(dest, src);
	}

	// TODO: Change this path
	sprintf(dest, "/home/mult/flashBench/src/log/log_resp_%d.dat", bus);

	for(i = 0 ; i < nr_timer_log_resp_records[bus] ; i++)
	{
		sprintf(src, "%d\n", timer_log_resp_records[bus][i]);
		fb_file_log(dest, src);
	}
#endif
	if(ptr_bus_controller->ptr_task != NULL)
	{
		kthread_stop(ptr_bus_controller->ptr_task);
	}
}


// ---------------- Static functions: Operation queue ----------------------------
// Creating an operation queue for a chip
static struct fb_opr_queue_t *create_opr_queue(u32 num_max_entries)
{
	struct fb_opr_queue_t *ptr_opr_queue;

	// Allocating an operation queue
	if((ptr_opr_queue =
				(struct fb_opr_queue_t *) kmalloc(sizeof(struct fb_opr_queue_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Allocating operation queue failed.\n");
		goto ALLOC_OPR_QUEUE_FAIL;
	}

	// Allocating an operation list where data are actually stored
	if((ptr_opr_queue->opr_list =
				(struct fb_operation_t *) kmalloc(sizeof(struct fb_operation_t) * num_max_entries, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "[FlashBench] bus controller: Allocating operation list failed.\n");
		goto ALLOC_OPR_LIST_FAIL;
	}

	// Initialize values: operation queues are based on circular queue.
	ptr_opr_queue->num_max_entries = num_max_entries;
	ptr_opr_queue->num_entries = 0;
	ptr_opr_queue->queue_head =
		ptr_opr_queue->queue_tail = 0;

	// Initialize a lock for the queue
	init_completion(&ptr_opr_queue->queue_lock);
	complete(&ptr_opr_queue->queue_lock);

	return ptr_opr_queue;


ALLOC_OPR_LIST_FAIL:
	if(ptr_opr_queue != NULL)
	{
		kfree(ptr_opr_queue);
	}

ALLOC_OPR_QUEUE_FAIL:
	return NULL;
}

// Destroying the operation queue
static void destroy_opr_queue(struct fb_opr_queue_t *ptr_opr_queue)
{
	if(ptr_opr_queue != NULL)
	{
		if(ptr_opr_queue->opr_list != NULL)
		{
			kfree(ptr_opr_queue->opr_list);
		}
		kfree(ptr_opr_queue);
	}
}

// Get the current number of entries in the target operation queue
static inline u32 opr_queue_num_entries(struct fb_opr_queue_t *ptr_opr_queue)
{
	return ptr_opr_queue->num_entries;
}

// Check whether the operation queue is full or not(full: TRUE, otherwise: FASLE)
static inline int opr_queue_full(struct fb_opr_queue_t *ptr_opr_queue)
{
	return (ptr_opr_queue->num_max_entries == opr_queue_num_entries(ptr_opr_queue)) ? TRUE : FALSE;
}

// Check whether the operation queue is empty or not(empty: TRUE, otherwise: FALSE)
static inline int opr_queue_empty(struct fb_opr_queue_t *ptr_opr_queue)
{
	return (opr_queue_num_entries(ptr_opr_queue) == 0) ? TRUE : FALSE;
}

// Put an entry into the target operation queue(success: 0, error: -1)
static int opr_queue_put_entry(
		struct fb_opr_queue_t *ptr_opr_queue,
		u32 operation,
		struct fb_bio_t *ptr_fb_bio)
{
	int ret = 0;
	struct fb_operation_t *ptr_opr_entry;

	// Acquiring the lock
	wait_for_completion(&ptr_opr_queue->queue_lock);
	//INIT_COMPLETION(ptr_opr_queue->queue_lock);
	reinit_completion(&ptr_opr_queue->queue_lock);

	// Check whether the queue is full or not
	// If it is full, return error for putting the request into it.
	if(opr_queue_full(ptr_opr_queue) == TRUE)
	{
		ret = -1;
		goto FINISH;
	}

	// Storing information in the first entry of the queue
	ptr_opr_entry = &ptr_opr_queue->opr_list[ptr_opr_queue->queue_head];

	ptr_opr_entry->operation = operation;
	ptr_opr_entry->ptr_fb_bio = ptr_fb_bio;

	// Marching of head pointer
	ptr_opr_queue->queue_head++;

	if(ptr_opr_queue->queue_head == ptr_opr_queue->num_max_entries)
	{
		ptr_opr_queue->queue_head = 0;
	}

	// Increasing the number of entries
	ptr_opr_queue->num_entries++;

FINISH:
	complete(&ptr_opr_queue->queue_lock);

	return ret;
}


// Get informations of the first entry in the target operation queue(success: 0, error: -1)
// (The first entry means that it is the oldes entry(first come).)
static int opr_queue_get_first(
		struct fb_opr_queue_t *ptr_opr_queue,
		u32 *ptr_operation,
		struct fb_bio_t **ptr_fb_bio)
{
	int ret = 0;
	struct fb_operation_t *ptr_opr_entry;

	// Acquiring the lock
	wait_for_completion(&ptr_opr_queue->queue_lock);
	//INIT_COMPLETION(ptr_opr_queue->queue_lock);
	reinit_completion(&ptr_opr_queue->queue_lock);

	// Error if the queue is emptry
	if(opr_queue_empty(ptr_opr_queue) == TRUE)
	{
		ret = -1;
		goto FINISH;
	}

	// Get information of the tail entry
	ptr_opr_entry = &ptr_opr_queue->opr_list[ptr_opr_queue->queue_tail];

	// Storing information
	*ptr_operation = ptr_opr_entry->operation;
	*ptr_fb_bio = ptr_opr_entry->ptr_fb_bio;

FINISH:
	complete(&ptr_opr_queue->queue_lock);

	return ret;
}

// Remove informations of the first entry in the target operation queue(success: 0, error: -1)
// (The first entry means that it is the oldes entry(first come).)
static int opr_queue_remove_first(
		struct fb_opr_queue_t *ptr_opr_queue)
{
	int ret = 0;

	// Acquiring the lock
	wait_for_completion(&ptr_opr_queue->queue_lock);
	//INIT_COMPLETION(ptr_opr_queue->queue_lock);
	reinit_completion(&ptr_opr_queue->queue_lock);

	// Error if the queue is emptry
	if(opr_queue_empty(ptr_opr_queue) == TRUE)
	{
		ret = -1;
		goto FINISH;
	}

	// Marching of the tail pointer
	ptr_opr_queue->queue_tail++;

	if(ptr_opr_queue->queue_tail == ptr_opr_queue->num_max_entries)
	{
		ptr_opr_queue->queue_tail = 0;
	}

	// Decrease the number of entries
	ptr_opr_queue->num_entries--;

FINISH:
	complete(&ptr_opr_queue->queue_lock);

	return ret;
}

static void release_busy_lock(
		struct fb_bus_controller_t *ptr_bus_controller,
		u32 chip)
{
	// Reset time values
	ptr_bus_controller->chip_busies[chip].wakeup_time_in_us = 0;
	ptr_bus_controller->chip_busies[chip].issue_time_in_us = 0;

	// Read block I/O management
	if(ptr_bus_controller->chip_busies[chip].ptr_fb_bio != NULL)
	{
		// Reduce the request count
		//ptr_bus_controller->chip_busies[chip].ptr_fb_bio->req_count--;

		// If request count is zero, it means that the block I/O completes
		//if(ptr_bus_controller->chip_busies[chip].ptr_fb_bio->req_count == 0)
		if(dec_bio_req_count (ptr_bus_controller->chip_busies[chip].ptr_fb_bio) == 0)
		{
			bio_endio(ptr_bus_controller->chip_busies[chip].ptr_fb_bio->bio);
			vfree(ptr_bus_controller->chip_busies[chip].ptr_fb_bio);
		}
		ptr_bus_controller->chip_busies[chip].ptr_fb_bio = NULL;
	}
}

static void acquire_busy_lock(
		struct fb_bus_controller_t *ptr_bus_controller,
		u32 chip,
		u32 operation,
		struct fb_bio_t *ptr_fb_bio)
{
	// Set time values
	ptr_bus_controller->chip_busies[chip].issue_time_in_us =
		timer_get_timestamp_in_us();
	ptr_bus_controller->chip_busies[chip].wakeup_time_in_us =
		ptr_bus_controller->chip_busies[chip].issue_time_in_us + operation_time(operation);

	ptr_bus_controller->chip_busies[chip].ptr_fb_bio = ptr_fb_bio;
}

static int chip_status_busy(
		struct fb_bus_controller_t *ptr_bus_controller, u32 chip)
{
	return (ptr_bus_controller->chip_busies[chip].wakeup_time_in_us == 0 &&
			ptr_bus_controller->chip_busies[chip].issue_time_in_us == 0) ? FALSE : TRUE;
}

static inline u32 get_chip_wakeup_time(
		struct fb_bus_controller_t *ptr_bus_controller, u32 chip)
{
	return ptr_bus_controller->chip_busies[chip].wakeup_time_in_us;
}

static inline u32 get_chip_issue_time(
		struct fb_bus_controller_t *ptr_bus_controller, u32 chip)
{
	return ptr_bus_controller->chip_busies[chip].issue_time_in_us;
}
