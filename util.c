#include <linux/buffer_head.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int fb_proc_open(struct inode *inode, struct file *file);
static int fb_proc_summary(struct seq_file *m, void *v);

static struct file *file_open(const char *path, int flags, int rights);
static void file_close(struct file *file);

// file operation 구조체, 초기화를 이렇게 함
// read 함수는 개발자가 직접 작성 X
// 커널의 seq-file에서 제공되는 seq_read를 사용
static const struct file_operations fb_proc_fops = {
    .owner = THIS_MODULE,
    .open = fb_proc_open,  // 파일(/proc/summary)을 열 때 불리는 함수
    .read = seq_read,  // 파일(/proc/summary)을 읽을 때 불리는 함수
};

static struct proc_dir_entry *proc_dir;

static u32 _perf_nr_incomming_write = 0;
static u32 _perf_nr_wordline_prog_fg = 0;
static u32 _perf_nr_wordline_prog_bg = 0;
static u32 _perf_nr_page_reads = 0;
static u32 _perf_nr_blk_erasures = 0;
static u32 _perf_nr_gc_trigger_fg = 0;
static u32 _perf_nr_gc_trigger_bg = 0;
static u32 _perf_nr_lsb_pg_backup = 0;
static u32 _perf_nr_discard_reqs = 0;
static u32 _perf_nr_discarded_lpgs = 0;
static u32 _perf_nr_plocks = 0;
static u32 _perf_nr_blocks = 0;

static int fb_proc_summary(struct seq_file *m,
                           __attribute__((unused)) void *v) {
  seq_printf(m,
             "===================Total read/write requests "
             "summary=================\n");
  seq_printf(m, "FlashBench: # of total write from OS: %u\n",
             _perf_nr_incomming_write);
  seq_printf(m, "FlashBench: # of read pages: %u\n", _perf_nr_page_reads);
  seq_printf(m, "FlashBench: # of write pages in FG procedures: %u\n",
             _perf_nr_wordline_prog_fg);
  seq_printf(m, "FlashBench: # of write pages in BG procedures: %u\n",
             _perf_nr_wordline_prog_bg);
  seq_printf(m, "FlashBench: # of erase blocks: %u\n", _perf_nr_blk_erasures);
  seq_printf(m, "FlashBench: # of FG GC: %u\n", _perf_nr_gc_trigger_fg);
  seq_printf(m, "FlashBench: # of BG GC: %u\n", _perf_nr_gc_trigger_bg);
  seq_printf(m, "FlashBench: # of LSB page backups: %u\n",
             _perf_nr_lsb_pg_backup);
  seq_printf(m, "FlashBench: # of discard reqs: %u (%u)\n",
             _perf_nr_discard_reqs, _perf_nr_discarded_lpgs);
  seq_printf(m, "FlashBench: # of plocks: %u\n", _perf_nr_plocks);
  seq_printf(m, "FlashBench: # of blocks: %u\n", _perf_nr_blocks);

  return 0;
}

void perf_inc_nr_incomming_write(void) { _perf_nr_incomming_write++; }

void perf_inc_nr_wordline_prog_fg(void) { _perf_nr_wordline_prog_fg++; }

void perf_inc_nr_wordline_prog_bg(void) { _perf_nr_wordline_prog_bg++; }

void perf_inc_nr_page_reads(void) { _perf_nr_page_reads++; }

void perf_inc_nr_blk_erasures(void) { _perf_nr_blk_erasures++; }

void perf_inc_nr_gc_trigger_bg(void) { _perf_nr_gc_trigger_bg++; }

void perf_inc_nr_discard_reqs(void) { _perf_nr_discard_reqs++; }

void perf_inc_nr_discard_lpgs(u32 nr_lpgs) {
  _perf_nr_discarded_lpgs += nr_lpgs;
}

u32 timer_get_timestamp_in_us(void) {
  struct timeval tv;

  do_gettimeofday(&tv);

  return tv.tv_sec * 1000000 + tv.tv_usec;
}

static int fb_proc_open(__attribute__((unused)) struct inode *inode,
                        struct file *file) {
  return single_open(file, fb_proc_summary, NULL);
}

static struct file *file_open(const char *path, int flags, int rights) {
  struct file *filp = NULL;
  mm_segment_t oldfs;
  int err = 0;

  oldfs = get_fs();
  set_fs(get_ds());
  filp = filp_open(path, flags, rights);
  set_fs(oldfs);
  if (IS_ERR(filp)) {
    err = PTR_ERR(filp);
    return NULL;
  }
  return filp;
}

static void file_close(struct file *file) { filp_close(file, NULL); }

static int file_write(struct file *file, const char *data, size_t size,
                      loff_t offset) {
  mm_segment_t oldfs;
  int ret;

  oldfs = get_fs();
  set_fs(get_ds());

  ret = kernel_write(file, data, size, offset);

  set_fs(oldfs);
  return ret;
}

void fb_file_log(const char *filename, const char *string) {
  struct file *fp = NULL;

  if ((fp = file_open(filename, O_CREAT | O_WRONLY | O_APPEND, 0777)) == NULL) {
    printk(KERN_INFO "blueftl_log: file_open error (%s)\n", filename);
    return;
  }

  file_write(fp, string, strlen(string), 0);

  file_close(fp);
}

void perf_display_result(void) {
  printk(KERN_INFO
         "===================Total read/write requests "
         "summary=================\n");
  printk(KERN_INFO "FlashBench: # of total write from OS: %u\n",
         _perf_nr_incomming_write);
  printk(KERN_INFO "FlashBench: # of read pages: %u\n", _perf_nr_page_reads);
  printk(KERN_INFO "FlashBench: # of write pages in FG procedures: %u\n",
         _perf_nr_wordline_prog_fg);
  printk(KERN_INFO "FlashBench: # of write pages in BG procedures: %u\n",
         _perf_nr_wordline_prog_bg);
  printk(KERN_INFO "FlashBench: # of erase blocks: %u\n",
         _perf_nr_blk_erasures);
  printk(KERN_INFO "FlashBench: # of FG GC: %u\n", _perf_nr_gc_trigger_fg);
  printk(KERN_INFO "FlashBench: # of BG GC: %u\n", _perf_nr_gc_trigger_bg);
  printk(KERN_INFO "FlashBench: # of LSB page backups: %u\n",
         _perf_nr_lsb_pg_backup);
  printk(KERN_INFO "FlashBench: # of discard reqs: %u (%u)\n",
         _perf_nr_discard_reqs, _perf_nr_discarded_lpgs);
}

void perf_init(void) {
  // procfs를 통해, 커널 정보를 사용자 영역에서 접근 가능하다.
  // proc_create(...): /proc 에다가 파일 생성하는 함수
  // 파일명, 권한, 디렉토리, file_operation 구조체

  proc_dir = proc_create("summary", 0444, NULL, &fb_proc_fops);

  if (proc_dir == NULL) {
    printk(KERN_INFO "proc summary creation failed \n");
    // TODO: Handle error properly
    // return -EEXIST;
  }
}

void perf_exit(void) { remove_proc_entry("summary", NULL); }
