#include <linux/bio.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "rust/libflashbench.h"

#include "main.h"
#include "bus_controller.h"

struct fb_operation_t {
  enum fb_dev_op_t operation;
  struct fb_bio_t *ptr_fb_bio;
};

struct fb_opr_queue_t {
  u32 num_max_entries;
  u32 num_entries;
  u32 queue_head;
  u32 queue_tail;
  struct fb_operation_t *opr_list;
  struct completion queue_lock;
};

struct fb_chip_busy_t {
  u32 wakeup_time_in_us;
  u32 issue_time_in_us;
  struct completion chip_busy;
  struct fb_bio_t *ptr_fb_bio;
};

struct fb_bus_controller_t {
  u32 num_bus;
  struct fb_opr_queue_t **ptr_opr_queue;
  struct fb_chip_busy_t *chip_busies;
  u32 flag_enable_thread;
  struct task_struct *ptr_task;
};

// ---------------- prototypes of static functions: bus controller
// ----------------------------
// Creating an bus controller for a bus
static struct fb_bus_controller_t *create_bus_controller(
    u32 num_max_entries_per_chip);

// Destroying the bus controller
static void destroy_bus_controller(
    struct fb_bus_controller_t *ptr_bus_controller);

// Bus controllers management thread
// Phase 1: Release bus locks whose conditions are satisfied
// Phase 2: Acquire bus locks for requests waiting for avaliable chips
static int fb_bus_ctrl_thread(void *arg);
static int fb_init_bus_ctrl_thread(
    struct fb_bus_controller_t *ptr_bus_controller, u32 bus);
static void fb_stop_bus_ctrl_thread(
    struct fb_bus_controller_t *ptr_bus_controller);
// ---------------- prototypes of static functions: operation queue
// ----------------------------

// Creating an operation queue for a chip
static struct fb_opr_queue_t *create_opr_queue(u32 num_max_entries);

// Destroying the operation queue
static void destroy_opr_queue(struct fb_opr_queue_t *ptr_opr_queue);

// Put an entry into the target operation queue(success: 0, error: -1)
static int opr_queue_put_entry(struct fb_opr_queue_t *ptr_opr_queue,
                               enum fb_dev_op_t operation, struct fb_bio_t *ptr_fb_bio);

// Get informations of the first entry in the target operation queue(success: 0,
// error: -1)
static int opr_queue_get_first(struct fb_opr_queue_t *ptr_opr_queue,
                               enum fb_dev_op_t *ptr_operation,
                               struct fb_bio_t **ptr_fb_bio);

// Remove informations of the first entry in the target operation queue(success:
// 0, error: -1)
static int opr_queue_remove_first(struct fb_opr_queue_t *ptr_opr_queue);

static void release_busy_lock(struct fb_bus_controller_t *ptr_bus_controller,
                              u32 chip);

static void acquire_busy_lock(struct fb_bus_controller_t *ptr_bus_controller,
                              u32 chip, enum fb_dev_op_t operation,
                              struct fb_bio_t *ptr_fb_bio);

static bool is_acquired(const struct fb_chip_busy_t *busy) {
  return busy->wakeup_time_in_us != 0 || busy->issue_time_in_us != 0;
}

static u32 timer_get_timestamp_in_us(void);


// ----------------- Public functions ----------------------------------------
// Creating and initialize bus controllers in the virtual device structure
int fb_bus_controller_init(struct vdevice_t *ptr_vdevice,
                           u32 num_max_entries_per_chip) {
  // Allocating a pointer of the bus controller list
  struct fb_bus_controller_t **ptr_bus_controller = kmalloc(sizeof(struct fb_bus_controller_t *) * NUM_BUSES, GFP_ATOMIC);
  if (!ptr_bus_controller) {
    printk(KERN_ERR
           "flashbench: bus controller: Allocating the pointer of bus "
           "controllers failed.\n");
    goto ALLOC_PTR_BUS_CTRL_FAIL;
  }

  // Creating bus controllers(for the number of buses)
  for (u32 loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
    ptr_bus_controller[loop_bus] = create_bus_controller(num_max_entries_per_chip);
    if (!ptr_bus_controller[loop_bus]) {
      printk(KERN_ERR
             "flashbench: bus controller: Creating bus controllers failed.\n");
      goto CREATE_BUS_CTRL_FAIL;
    }
  }

  // Initilaize bus control threads(for the number of buses)
  for (u32 loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
    ptr_bus_controller[loop_bus]->flag_enable_thread = 1;
    ptr_bus_controller[loop_bus]->num_bus = loop_bus;

    if (fb_init_bus_ctrl_thread(ptr_bus_controller[loop_bus], loop_bus) == -1) {
      goto INIT_BUS_CTRL_THREAD_FAIL;
    }
  }

  ptr_vdevice->ptr_bus_controller = ptr_bus_controller;

  return 0;

INIT_BUS_CTRL_THREAD_FAIL:
  for (u32 loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
    ptr_bus_controller[loop_bus]->flag_enable_thread = 0;
    fb_stop_bus_ctrl_thread(ptr_bus_controller[loop_bus]);
  }

CREATE_BUS_CTRL_FAIL:
  if (ptr_bus_controller) {
    for (u32 loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
      if (!ptr_bus_controller[loop_bus]) { break; }
      destroy_bus_controller(ptr_bus_controller[loop_bus]);
    }

    kfree(ptr_bus_controller);
  }
ALLOC_PTR_BUS_CTRL_FAIL:
  return -1;
}

// Destory bus controllers
void fb_bus_controller_destroy(
    struct fb_bus_controller_t **ptr_bus_controller) {
  if (!ptr_bus_controller) { return; }

  for (u32 loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
    if (!ptr_bus_controller[loop_bus]) { continue; }
    ptr_bus_controller[loop_bus]->flag_enable_thread = 0;
    fb_stop_bus_ctrl_thread(ptr_bus_controller[loop_bus]);
  }

  for (u32 loop_bus = 0; loop_bus < NUM_BUSES; loop_bus++) {
    if (!ptr_bus_controller[loop_bus]) { continue; }
    destroy_bus_controller(ptr_bus_controller[loop_bus]);
  }

  kfree(ptr_bus_controller);
}

// Issue an operation for the target chip of the target bus
void fb_issue_operation(struct fb_bus_controller_t *ptr_bus_controller, u32 chip,
                       enum fb_dev_op_t operation, struct fb_bio_t *ptr_bio) {
  while (opr_queue_put_entry(ptr_bus_controller->ptr_opr_queue[chip], operation,
                             ptr_bio) == -1);
}

// ---------------- Static functions: Bus contorller
// ----------------------------
// Creating an bus controller for a bus
static struct fb_bus_controller_t *create_bus_controller(
    u32 num_max_entries_per_chip) {
  // Allocating a pointer of bus contorller structure
  struct fb_bus_controller_t *ptr_bus_controller = kmalloc(sizeof(struct fb_bus_controller_t), GFP_ATOMIC);
  if (!ptr_bus_controller) {
    printk(KERN_ERR
           "flashbench: bus controller: Allocating bus controller failed.\n");
    goto ALLOC_BUS_CTRL_FAIL;
  }

  // Allocating a pointer of operation queue list(for number of chips)
  ptr_bus_controller->ptr_opr_queue = kmalloc(sizeof(struct fb_opr_queue_t *) * NUM_CHIPS_PER_BUS, GFP_ATOMIC);
  if (!ptr_bus_controller->ptr_opr_queue) {
    printk(KERN_ERR
           "flashbench: bus controller: Allocating pointer of operation "
           "queues failed.\n");
    goto ALLOC_PTR_OPR_QUEUE_FAIL;
  }

  // Creating operation queues
  for (u32 loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
    ptr_bus_controller->ptr_opr_queue[loop_chip] = create_opr_queue(num_max_entries_per_chip);
    if (!ptr_bus_controller->ptr_opr_queue[loop_chip]) {
      printk(KERN_ERR
             "flashbench: bus controller: Creating operation queues failed.\n");
      goto CREATE_OPR_QUEUE_FAIL;
    }
  }

  // Creating chip busy structures
  ptr_bus_controller->chip_busies = kmalloc(sizeof(struct fb_chip_busy_t) * NUM_CHIPS_PER_BUS, GFP_ATOMIC);
  if (!ptr_bus_controller->chip_busies) {
    printk(KERN_ERR
           "flashbench: bus controller: Allocating chip busy array failed.\n");
    goto CREATE_OPR_QUEUE_FAIL;
  }

  // Initialize chip busy structrues
  for (u32 loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
    init_completion(&ptr_bus_controller->chip_busies[loop_chip].chip_busy);
    complete(&ptr_bus_controller->chip_busies[loop_chip].chip_busy);

    ptr_bus_controller->chip_busies[loop_chip].wakeup_time_in_us = 0;
    ptr_bus_controller->chip_busies[loop_chip].issue_time_in_us = 0;
    ptr_bus_controller->chip_busies[loop_chip].ptr_fb_bio = NULL;
  }

  // end of the function
  return ptr_bus_controller;

CREATE_OPR_QUEUE_FAIL:
  if (ptr_bus_controller->ptr_opr_queue) {
    for (u32 loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
      if (!ptr_bus_controller->ptr_opr_queue[loop_chip]) { break; }
      destroy_opr_queue(ptr_bus_controller->ptr_opr_queue[loop_chip]);
    }

    kfree(ptr_bus_controller->ptr_opr_queue);
  }

ALLOC_PTR_OPR_QUEUE_FAIL:
  if (ptr_bus_controller) {
    kfree(ptr_bus_controller);
  }

ALLOC_BUS_CTRL_FAIL:
  return NULL;
}

// Destroying the bus controller
static void destroy_bus_controller(
    struct fb_bus_controller_t *ptr_bus_controller) {
  if (ptr_bus_controller) {
    if (ptr_bus_controller->chip_busies) {
      kfree(ptr_bus_controller->chip_busies);
    }

    if (ptr_bus_controller->ptr_opr_queue) {
      for (u32 loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
        if (!ptr_bus_controller->ptr_opr_queue[loop_chip]) { break; }
        destroy_opr_queue(ptr_bus_controller->ptr_opr_queue[loop_chip]);
      }

      kfree(ptr_bus_controller->ptr_opr_queue);
    }

    kfree(ptr_bus_controller);
  }
}

// Bus controllers management thread
// Phase 1: Release bus locks whose conditions are satisfied
// Phase 2: Acquire bus locks for requests waiting for avaliable chips
static int fb_bus_ctrl_thread(void *arg) {
  struct fb_bus_controller_t *ptr_bus_controller = (struct fb_bus_controller_t *)arg;

  allow_signal(SIGKILL);
  allow_signal(SIGSTOP);
  allow_signal(SIGCONT);

  ptr_bus_controller->ptr_task = current;

  set_user_nice(current, -19);
  set_freezable();

  while ((ptr_bus_controller->flag_enable_thread) && !kthread_should_stop()) {
    u32 signr = 0;

    allow_signal(SIGHUP);

  AGAIN:
    while (signal_pending(current)) {
      if (try_to_freeze()) {
        goto AGAIN;
      }

      signr = kernel_dequeue_signal(NULL);

      switch (signr) {
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
    u32 current_time_in_us = timer_get_timestamp_in_us();

    // Release bus locks whose conditions are satisfied
    for (u32 loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
      // Get wakeup time, and check it is valid value.
      u32 wakeup_time_in_us = ptr_bus_controller->chip_busies[loop_chip].wakeup_time_in_us;
      if (wakeup_time_in_us > 0) {
        // Check the condition
        if (wakeup_time_in_us <= current_time_in_us) {
          // Release lock
          // If ptr_fb_bio for the chip is not null, the request count
          // decreases.
          // And if the request becomes zero, bio will be returned.
          release_busy_lock(ptr_bus_controller, loop_chip);
        }
      }
    }

    // Phase 2: Acquire bus locks for requests waiting for avaliable chips
    for (u32 loop_chip = 0; loop_chip < NUM_CHIPS_PER_BUS; loop_chip++) {
      // Check whether the operation queue for the chip is empty or not,
      // and the chip is available or not.
      if (ptr_bus_controller->ptr_opr_queue[loop_chip]->num_entries == 0 ||
          is_acquired(&ptr_bus_controller->chip_busies[loop_chip])) {
        continue;
      }

      // Get informations of the first request for the chip
      enum fb_dev_op_t operation;
      struct fb_bio_t *ptr_fb_bio = NULL;
      if (opr_queue_get_first(ptr_bus_controller->ptr_opr_queue[loop_chip],
                              &operation, &ptr_fb_bio) == -1) {
        printk(KERN_ERR
               "flashbench: bus controller: Operation queue should not be "
               "empty.\n");
        goto FINISH;
      }

      // Issue the request
      acquire_busy_lock(ptr_bus_controller, loop_chip, operation, ptr_fb_bio);

      // Remove the entry
      opr_queue_remove_first(ptr_bus_controller->ptr_opr_queue[loop_chip]);
    }

    if (ptr_bus_controller->flag_enable_thread == 0) {
      yield();
      goto FINISH;
    }

    yield();
  }

FINISH:
  printk(KERN_INFO "flashbench: bus controller: End condition of thread\n");
  ptr_bus_controller->flag_enable_thread = 0;
  ptr_bus_controller->ptr_task = NULL;

  return 0;
}

static int fb_init_bus_ctrl_thread(
    struct fb_bus_controller_t *ptr_bus_controller, u32 bus) {
  char thread_name[16];
  sprintf(thread_name, "fb_bus_%d", bus);

  ptr_bus_controller->ptr_task = kthread_run(fb_bus_ctrl_thread, ptr_bus_controller, thread_name);
  int rc = IS_ERR(ptr_bus_controller->ptr_task);

  if (rc < 0) {
    printk(KERN_ERR
           "flashbench: bus controller: Creating bus %d control task failed.\n",
           bus);
    return -1;
  } else {
    printk(KERN_INFO
           "flashbench: bus controller: Bus %d controller is successfully "
           "created.\n",
           bus);
    return 0;
  }
}

static void fb_stop_bus_ctrl_thread(
    struct fb_bus_controller_t *ptr_bus_controller) {
  if (!ptr_bus_controller->ptr_task) { return; }
  kthread_stop(ptr_bus_controller->ptr_task);
}

// ---------------- Static functions: Operation queue
// ----------------------------
// Creating an operation queue for a chip
static struct fb_opr_queue_t *create_opr_queue(u32 num_max_entries) {
  // Allocating an operation queue
  struct fb_opr_queue_t *ptr_opr_queue = kmalloc(sizeof(struct fb_opr_queue_t), GFP_ATOMIC);
  if (!ptr_opr_queue) {
    printk(KERN_ERR
           "flashbench: bus controller: Allocating operation queue failed.\n");
    goto ALLOC_OPR_QUEUE_FAIL;
  }

  // Allocating an operation list where data are actually stored
  ptr_opr_queue->opr_list = kmalloc(sizeof(struct fb_operation_t) * num_max_entries, GFP_ATOMIC);
  if (!ptr_opr_queue->opr_list) {
    printk(KERN_ERR
           "flashbench: bus controller: Allocating operation list failed.\n");
    goto ALLOC_OPR_LIST_FAIL;
  }

  // Initialize values: operation queues are based on circular queue.
  ptr_opr_queue->num_max_entries = num_max_entries;
  ptr_opr_queue->num_entries = 0;
  ptr_opr_queue->queue_head = 0;
  ptr_opr_queue->queue_tail = 0;

  // Initialize a lock for the queue
  init_completion(&ptr_opr_queue->queue_lock);
  complete(&ptr_opr_queue->queue_lock);

  return ptr_opr_queue;

ALLOC_OPR_LIST_FAIL:
  if (ptr_opr_queue) {
    kfree(ptr_opr_queue);
  }

ALLOC_OPR_QUEUE_FAIL:
  return NULL;
}

// Destroying the operation queue
static void destroy_opr_queue(struct fb_opr_queue_t *ptr_opr_queue) {
  if (!ptr_opr_queue) { return; }
  if (ptr_opr_queue->opr_list) { kfree(ptr_opr_queue->opr_list); }
  kfree(ptr_opr_queue);
}

// Put an entry into the target operation queue(success: 0, error: -1)
static int opr_queue_put_entry(struct fb_opr_queue_t *ptr_opr_queue,
                               enum fb_dev_op_t operation, struct fb_bio_t *ptr_fb_bio) {
  int ret = 0;

  // Acquiring the lock
  wait_for_completion(&ptr_opr_queue->queue_lock);
  reinit_completion(&ptr_opr_queue->queue_lock);

  // Check whether the queue is full or not
  // If it is full, return error for putting the request into it.
  if (ptr_opr_queue->num_entries == ptr_opr_queue->num_max_entries) {
    ret = -1;
    goto FINISH;
  }

  // Storing information in the first entry of the queue
  struct fb_operation_t *ptr_opr_entry = &ptr_opr_queue->opr_list[ptr_opr_queue->queue_head];

  ptr_opr_entry->operation = operation;
  ptr_opr_entry->ptr_fb_bio = ptr_fb_bio;
  // Marching of head pointer
  ptr_opr_queue->queue_head++;

  if (ptr_opr_queue->queue_head == ptr_opr_queue->num_max_entries) {
    ptr_opr_queue->queue_head = 0;
  }

  // Increasing the number of entries
  ptr_opr_queue->num_entries++;

FINISH:
  complete(&ptr_opr_queue->queue_lock);

  return ret;
}

// Get informations of the first entry in the target operation queue(success: 0,
// error: -1)
// (The first entry means that it is the oldes entry(first come).)
static int opr_queue_get_first(struct fb_opr_queue_t *ptr_opr_queue,
                               enum fb_dev_op_t *ptr_operation,
                               struct fb_bio_t **ptr_fb_bio) {
  int ret = 0;

  // Acquiring the lock
  wait_for_completion(&ptr_opr_queue->queue_lock);
  reinit_completion(&ptr_opr_queue->queue_lock);

  // Error if the queue is emptry
  if (ptr_opr_queue->num_entries == 0) {
    ret = -1;
    goto FINISH;
  }

  // Get information of the tail entry
  struct fb_operation_t *ptr_opr_entry = &ptr_opr_queue->opr_list[ptr_opr_queue->queue_tail];

  // Storing information
  *ptr_operation = ptr_opr_entry->operation;
  *ptr_fb_bio = ptr_opr_entry->ptr_fb_bio;

FINISH:
  complete(&ptr_opr_queue->queue_lock);

  return ret;
}

// Remove informations of the first entry in the target operation queue(success:
// 0, error: -1)
// (The first entry means that it is the oldes entry(first come).)
static int opr_queue_remove_first(struct fb_opr_queue_t *ptr_opr_queue) {
  int ret = 0;

  // Acquiring the lock
  wait_for_completion(&ptr_opr_queue->queue_lock);
  reinit_completion(&ptr_opr_queue->queue_lock);

  // Error if the queue is emptry
  if (ptr_opr_queue->num_entries == 0) {
    ret = -1;
    goto FINISH;
  }

  // Marching of the tail pointer
  ptr_opr_queue->queue_tail++;

  if (ptr_opr_queue->queue_tail == ptr_opr_queue->num_max_entries) {
    ptr_opr_queue->queue_tail = 0;
  }

  // Decrease the number of entries
  ptr_opr_queue->num_entries--;

FINISH:
  complete(&ptr_opr_queue->queue_lock);

  return ret;
}

static void release_busy_lock(struct fb_bus_controller_t *ptr_bus_controller,
                              u32 chip) {
  struct fb_chip_busy_t *busy = &ptr_bus_controller->chip_busies[chip];

  // Reset time values
  busy->wakeup_time_in_us = 0;
  busy->issue_time_in_us = 0;

  // Read block I/O management
  if (!busy->ptr_fb_bio) { return; }

  // Reduce the request count. If request count is zero, it means that the
  // block I/O completes
  if (dec_bio_req_count(busy->ptr_fb_bio) == 0) {
    bio_endio(busy->ptr_fb_bio->bio);
    vfree(busy->ptr_fb_bio);
  }
  busy->ptr_fb_bio = NULL;
}

static void acquire_busy_lock(struct fb_bus_controller_t *ptr_bus_controller,
                              u32 chip, enum fb_dev_op_t operation,
                              struct fb_bio_t *ptr_fb_bio) {
  struct fb_chip_busy_t *busy = &ptr_bus_controller->chip_busies[chip];

  // Set time values
  const u32 time = timer_get_timestamp_in_us();

  busy->issue_time_in_us = time;
  busy->wakeup_time_in_us = time + operation_time(operation);
  busy->ptr_fb_bio = ptr_fb_bio;
}

static u32 timer_get_timestamp_in_us(void) {
  struct timeval tv;
  do_gettimeofday(&tv);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}
