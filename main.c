#include <linux/blkdev.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include "rust/libflashbench.h"

#include "ftl_algorithm_page_mapping.h"
#include "main.h"
#include "option.h"
#include "ssd_info.h"
#include "util.h"
#include "write_buffer.h"

//
// Prototypes
//
static int fb_init_write_buffer_thread(void);
static int fb_write_buffer_thread(void *arg);
static u64 fb_get_time_in_us(void);
static void fb_update_bgc_ts(struct fb_context_t *fb);
static int fb_is_bgc_ts_expired(struct fb_context_t *fb, u64 threshold);
static void destroy_mapping_context(struct fb_context_t *ptr_fb_context);
u32 dec_bio_req_count(struct fb_bio_t *ptr_bio);
static struct fb_bio_t *fb_build_bio(struct bio *bio);
static void fb_destroy_bio(struct fb_bio_t *fbio);
static blk_qc_t make_request(struct request_queue *ptr_req_queue,
                             struct bio *bio);
static int __init fb_init(void);
static void __exit fb_exit(void);

//
// Global Variables
//
static const char *DEV_NAME = "fbSSD";
static struct fb_context_t *_fb;
static struct block_device_operations bdops = {.owner = THIS_MODULE};

//
// Function definitions
//
static blk_qc_t make_request(
    __attribute__((unused)) struct request_queue *ptr_req_queue,
    struct bio *bio) {
  const int rw =
      bio_data_dir(bio);  // data direction을 돌려줌. read인지 write인지
  u32 ret_value = 0;
  u32 loop, req_count;

  struct fb_bio_t *fbio = NULL;

  // TODO: Race condition! wait_for_completion + reinit_completion is NOT
  // atomic!
  wait_for_completion(&_fb->dev_lock);
  reinit_completion(&_fb->dev_lock);

  fb_update_bgc_ts(_fb);

  if (_fb->err == true) goto FAIL;

  if (unlikely(bio->bi_opf & REQ_PREFLUSH)) {
    _fb->make_flush_request();
    goto REQ_FINISH;
  }

  if (unlikely(bio_op(bio) == REQ_OP_DISCARD)) {
    _fb->make_discard_request(_fb, bio);
    goto REQ_FINISH;
  }

  if ((fbio = fb_build_bio(bio)) == NULL) {
    printk(KERN_ERR "flashbench: Building bio structure failed.\n");
    ret_value = -ENODEV;
    goto FAIL;
  }

  req_count = fbio->req_count;

  switch (rw) {
    case READ:

      if (fbio->req_count == 0) {
        bio_endio(fbio->bio);
        fb_destroy_bio(fbio);

        goto REQ_FINISH;
      }

      for (loop = 0; loop < req_count; loop++) {
        if (_fb->make_read_request(_fb, fbio->lpas[loop], fbio->kpages[loop],
                                   fbio) == -1) {
          if (dec_bio_req_count(fbio) == 0) {
            bio_endio(fbio->bio);
            fb_destroy_bio(fbio);
          }
        }
      }

      break;

    case WRITE:

      fb_del_invalid_data(_fb, fbio);

      for (loop = 0; loop < req_count; loop++) {
        if (fb_put_pg(_fb->wb, fbio->lpas[loop],
                      fbio->kpages[loop]) == 0) {
          perf_inc_nr_incomming_write();
        } else {
          // flush write buffer
          if (_fb->wb_flush(_fb) == -1) {
            printk(KERN_ERR "flashbench: WB flushing failed.\n");
            goto FAIL;
          }
          // retry
          loop--;
        }
      }

      break;

    default:
      printk(KERN_ERR "flashbench: Invalid I/O type (%d)\n", rw);
      ret_value = -ENOMSG;
      goto FAIL;
  }

REQ_FINISH:
  if (rw != READ) {
    bio_endio(bio);
    if (fbio != NULL) fb_destroy_bio(fbio);
  }

  fb_update_bgc_ts(_fb);

  complete(&_fb->dev_lock);

  return BLK_QC_T_NONE;

FAIL:
  _fb->err = true;

  bio->bi_error = ret_value;
  bio_endio(bio);
  if (fbio != NULL) fb_destroy_bio(fbio);
  complete(&_fb->dev_lock);
  return BLK_QC_T_NONE;
}

// 처음 insmod 시 flashbench 초기화
static int __init
fb_init(void)  // __init: 해당 함수 혹은 변수가 초기화 과정에서만 사용됨을 의미.
{
  int ret_value = 0;

  perf_init();  // procfs에 summary 파일 생성 및 사용을 위한 초기화

  if ((_fb = (struct fb_context_t *)kmalloc(sizeof(struct fb_context_t),
                                            GFP_ATOMIC)) == NULL) {
    printk(KERN_ERR
           "flashbench: Memory allocation for FlashBench context failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_ALLOC_CONTEXT;
  }

  if ((_fb->ptr_vdevice = create_vdevice()) == NULL) {
    printk(KERN_ERR "flashbench: Creating a virtual device failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_CREATE_VDEVICE;
  }

  if ((_fb->ptr_ssd_info = create_ssd_info()) == NULL) {
    printk(KERN_ERR
           "flashbench: Creating information structure of virtual device "
           "failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_CREATE_SSD_INFO;
  }

  if ((_fb->ptr_mapping_context = create_pg_ftl(_fb)) == NULL) {
    printk(KERN_ERR "flashbench: Creating a mapping context failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_CREATE_MAPPING_CONTEXT;
  }

  if (!(_fb->ptr_req_queue = blk_alloc_queue(GFP_KERNEL))) {
    printk(KERN_ERR "flashbench: Allocating a block queue failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_ALLOC_BDEV_QUEUE;
  }

  // Init background GC timestamp
  _fb->background_gc_time_stamp = 0;

  init_completion(&_fb->dev_lock);
  complete(&_fb->dev_lock);

  _fb->device_major_num = 0;

  // 해당 request queue의 요청을 처리할 함수를 등록한다.
  blk_queue_make_request(_fb->ptr_req_queue, make_request);
  // sector size 정의, 기본읜 512B고 여기서는 4KB로 잡음
  // TODO: blk_queue_physical_block_size() 함수는 안써도 되나...?
  blk_queue_logical_block_size(_fb->ptr_req_queue, LOGICAL_PAGE_SIZE);

  blk_queue_io_min(_fb->ptr_req_queue, LOGICAL_PAGE_SIZE);
  // 항상 PAGE_SIZE에 align된, n*PAGE_SIZE 크기의 I/O req를 받기위함
  blk_queue_io_opt(_fb->ptr_req_queue, LOGICAL_PAGE_SIZE);

  // Discard 단위
  _fb->ptr_req_queue->limits.discard_granularity = LOGICAL_PAGE_SIZE;
  _fb->ptr_req_queue->limits.max_discard_sectors = UINT_MAX;
  // 이전에 Discard한 Data를 다시 read할 때,
  // stale or random Data를 돌려주는 것이 아니라,
  // zero fill된 Data를 돌려주도록 한다.
  // file system이 해당 Data가 Clear되있기를 기대할 수도 있기 때문이다.
  _fb->ptr_req_queue->limits.discard_zeroes_data = 1;

  // 잘은 모르겠고 DISCARD를 support한다는 것으로 추정
  queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, _fb->ptr_req_queue);

  if ((_fb->device_major_num =
           register_blkdev(_fb->device_major_num, DEV_NAME)) < 0) {
    printk(KERN_ERR "flashbench: Registering a block device failed.\n");
    ret_value = _fb->device_major_num;
    goto FAIL_REGISTER_BDEV;
  }

  // Logical Disk를 등록시키기 위한 자료구조인 gendisk를 할당받는다
  if (!(_fb->gd = alloc_disk(1))) {
    printk(KERN_ERR "flashbench: Allocating a disk failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_ALLOC_DISK;
  }

  // TODO GH
  if ((_fb->wb = fb_create_write_buffer(NUM_PAGES_IN_WRITE_BUFFER,
                                        LOGICAL_PAGE_SIZE)) == NULL) {
    printk(KERN_ERR "flashbench: Creating a write buffer failed.\n");
    ret_value = -ENOMEM;
    goto FAIL_CREATE_WRITE_BUFFER;
  }

  _fb->flag_enable_wb_thread = 1;
  if (fb_init_write_buffer_thread() == -1) {
    printk(KERN_ERR "flashbench: Creating write buffer thread failed.\n");
    goto FAIL_WRITE_BUFFER_THREAD;
  }

  _fb->gd->major = _fb->device_major_num;
  _fb->gd->first_minor = 0;
  _fb->gd->fops = &bdops;
  _fb->gd->queue = _fb->ptr_req_queue;
  _fb->gd->private_data = NULL;
  strcpy(_fb->gd->disk_name, DEV_NAME);

  set_capacity(_fb->gd, _fb->ptr_vdevice->logical_capacity / SECTOR_SIZE);
  add_disk(_fb->gd);

  return 0;

FAIL_WRITE_BUFFER_THREAD:
  if (_fb->wb != NULL) {
    fb_destroy_write_buffer(_fb->wb);
  }

FAIL_CREATE_WRITE_BUFFER:
  del_gendisk(_fb->gd);

FAIL_ALLOC_DISK:
  unregister_blkdev(_fb->device_major_num, DEV_NAME);

FAIL_REGISTER_BDEV:
  blk_cleanup_queue(_fb->ptr_req_queue);

FAIL_ALLOC_BDEV_QUEUE:
  if (_fb->ptr_mapping_context != NULL) {
    destroy_mapping_context(_fb);
  }

FAIL_CREATE_MAPPING_CONTEXT:
  if (_fb->ptr_ssd_info != NULL) {
    destroy_ssd_info(_fb->ptr_ssd_info);
  }

FAIL_CREATE_SSD_INFO:
  if (_fb->ptr_vdevice != NULL) {
    destroy_vdevice(_fb->ptr_vdevice);
  }

FAIL_CREATE_VDEVICE:
  if (_fb != NULL) {
    kfree(_fb);
  }

FAIL_ALLOC_CONTEXT:
  return ret_value;
}

static void __exit fb_exit(void) {
  del_gendisk(_fb->gd);
  put_disk(_fb->gd);
  unregister_blkdev(_fb->device_major_num, DEV_NAME);

  blk_cleanup_queue(_fb->ptr_req_queue);
  _fb->flag_enable_wb_thread = 0;
  // Stop write buffer thread
  if (_fb->ptr_wb_task != NULL) {
    kthread_stop(_fb->ptr_wb_task);
  }

  if (_fb->wb != NULL) {
    fb_destroy_write_buffer(_fb->wb);
  }

  if (_fb->ptr_mapping_context != NULL) {
    destroy_mapping_context(_fb);
  }

  if (_fb->ptr_ssd_info != NULL) {
    destroy_ssd_info(_fb->ptr_ssd_info);
  }

  if (_fb->ptr_vdevice != NULL) {
    destroy_vdevice(_fb->ptr_vdevice);
  }

  if (_fb != NULL) {
    kfree(_fb);
  }

  perf_display_result();

  perf_exit();
}

static int fb_init_write_buffer_thread(void) {
  int rc;
  int ret = 0;

  char thread_name[16];
  sprintf(thread_name, "fb_wb_thread");

  _fb->ptr_wb_task = kthread_run(fb_write_buffer_thread, NULL, thread_name);
  rc = IS_ERR(_fb->ptr_wb_task);

  if (rc < 0) {
    ret = -1;
  }

  return ret;
}

static int fb_write_buffer_thread(__attribute__((unused)) void *arg) {
  u32 signr;
  int ret = 0;

  allow_signal(SIGKILL);
  allow_signal(SIGSTOP);
  allow_signal(SIGCONT);

  _fb->ptr_wb_task = current;

  set_user_nice(current, -19);
  set_freezable();

  while ((_fb->flag_enable_wb_thread) && !kthread_should_stop()) {
    signr = 0;
    ret = 0;

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

    if (fb_is_bgc_ts_expired(_fb, BGC_TH_INTV) == true) {
      if (_fb->background_gc(_fb) == -1) {
        printk(KERN_ERR "flashbench: BGC falied.\n");
        goto FINISH;
      }
    }

    if (_fb->flag_enable_wb_thread == 0) {
      yield();
      goto FINISH;
    }

    yield();
  }

FINISH:
  printk(KERN_INFO "flashbench: End condition of write buffer thread\n");
  _fb->flag_enable_wb_thread = 0;
  _fb->ptr_wb_task = NULL;

  return 0;
}

struct ssd_info *get_ssd_inf(struct fb_context_t *fb) {
  return fb->ptr_ssd_info;
}

struct vdevice_t *get_vdev(struct fb_context_t *fb) {
  return fb->ptr_vdevice;
}

struct page_mapping_context_t *get_ftl(struct fb_context_t *fb) {
  return fb->ptr_mapping_context;
}

static u64 fb_get_time_in_us(void) { return ktime_to_us(ktime_get()); }

static void fb_update_bgc_ts(struct fb_context_t *fb) {
  fb->background_gc_time_stamp = fb_get_time_in_us();
}

static int fb_is_bgc_ts_expired(struct fb_context_t *fb, u64 threshold) {
  return fb_get_time_in_us() - fb->background_gc_time_stamp > threshold;
}

static void destroy_mapping_context(struct fb_context_t *ptr_fb_context) {
  struct page_mapping_context_t *ctxt = get_ftl(ptr_fb_context);
  destroy_pg_ftl(ctxt);
}

u32 dec_bio_req_count(struct fb_bio_t *fbio) {
  u32 ret;

  // TODO: Race condition! wait_for_completion + reinit_completion is NOT
  // atomic!
  wait_for_completion(&fbio->bio_lock);
  reinit_completion(&fbio->bio_lock);

  fbio->req_count--;
  ret = fbio->req_count;

  complete(&fbio->bio_lock);

  return ret;
}

static struct fb_bio_t *fb_build_bio(struct bio *bio) {
  struct bio_vec bvec;
  const int rw = bio_data_dir(bio);
  struct bvec_iter bio_loop;
  u64 sec_start, lpa_curr;
  struct fb_bio_t *fbio = NULL;
  u8 *ptr_page_buffer;

  if ((fbio = (struct fb_bio_t *)vmalloc(sizeof(struct fb_bio_t))) == NULL) {
    printk(KERN_ERR "flashbench: Allocating bio structure failed.\n");
    return NULL;
  }

  fbio->bio = bio;
  fbio->req_count = 0;

  // assumption: logical page size (i.e., mapping size) = 4 KB
  sec_start = bio->bi_iter.bi_sector & (~(7));

  bio_for_each_segment(bvec, bio, bio_loop) {
    lpa_curr = sec_start >> 3;
    ptr_page_buffer = (u8 *)page_address(bvec.bv_page);

    if (rw == READ) {
      if (fb_get_pg_data(_fb->wb, lpa_curr, ptr_page_buffer) !=
          -1) {
        sec_start += 8;
        continue;
      }
    }

    fbio->lpas[fbio->req_count] = lpa_curr;
    fbio->kpages[fbio->req_count] = ptr_page_buffer;
    fbio->req_count++;

    sec_start += 8;
  }

  init_completion(&fbio->bio_lock);
  complete(&fbio->bio_lock);

  return fbio;
}

static void fb_destroy_bio(struct fb_bio_t *fbio) { vfree(fbio); }

MODULE_LICENSE("GPL");
module_init(fb_init) module_exit(fb_exit)
