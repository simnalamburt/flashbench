#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "util.h"

static int fb_proc_open(struct inode *inode, struct file *file);
static int fb_proc_show(struct seq_file *m, void *v);

// file operation 구조체, 초기화를 이렇게 함
// read 함수는 개발자가 직접 작성 X
// 커널의 seq-file에서 제공되는 seq_read를 사용
static const struct file_operations fb_proc_fops = {
  .owner = THIS_MODULE,
  .open = fb_proc_open,
  .read = seq_read,
};

static u32 _perf_nr_incomming_write = 0;
static u32 _perf_nr_wordline_prog_fg = 0;
static u32 _perf_nr_wordline_prog_bg = 0;
static u32 _perf_nr_page_reads = 0;
static u32 _perf_nr_blk_erasures = 0;
static u32 _perf_nr_gc_trigger_bg = 0;
static u32 _perf_nr_discard_reqs = 0;
static u32 _perf_nr_discarded_lpgs = 0;

void perf_inc_nr_incomming_write(void) { _perf_nr_incomming_write++; }
void perf_inc_nr_wordline_prog_fg(void) { _perf_nr_wordline_prog_fg++; }
void perf_inc_nr_wordline_prog_bg(void) { _perf_nr_wordline_prog_bg++; }
void perf_inc_nr_page_reads(void) { _perf_nr_page_reads++; }
void perf_inc_nr_blk_erasures(void) { _perf_nr_blk_erasures++; }
void perf_inc_nr_gc_trigger_bg(void) { _perf_nr_gc_trigger_bg++; }
void perf_inc_nr_discard_reqs(void) { _perf_nr_discard_reqs++; }
void perf_inc_nr_discard_lpgs(u32 nr_lpgs) { _perf_nr_discarded_lpgs += nr_lpgs; }

void perf_display_result(void) {
  printk(
      KERN_INFO
      "flashbench: Statistics of Flashbench module:\n"
      "flashbench: # of total write from OS: %u\n"
      "flashbench: # of read pages: %u\n"
      "flashbench: # of write pages in FG procedures: %u\n"
      "flashbench: # of write pages in BG procedures: %u\n"
      "flashbench: # of erase blocks: %u\n"
      "flashbench: # of BG GC: %u\n"
      "flashbench: # of discard reqs: %u (%u)\n",
      _perf_nr_incomming_write, _perf_nr_page_reads, _perf_nr_wordline_prog_fg,
      _perf_nr_wordline_prog_bg, _perf_nr_blk_erasures, _perf_nr_gc_trigger_bg,
      _perf_nr_discard_reqs, _perf_nr_discarded_lpgs);
}

static int fb_proc_show(struct seq_file *m,
                           __attribute__((unused)) void *v) {
  seq_printf(
      m,
      "Statistics of Flashbench module:\n"
      "# of total write from OS: %u\n"
      "# of read pages: %u\n"
      "# of write pages in FG procedures: %u\n"
      "# of write pages in BG procedures: %u\n"
      "# of erase blocks: %u\n"
      "# of BG GC: %u\n"
      "# of discard reqs: %u (%u)\n",
      _perf_nr_incomming_write, _perf_nr_page_reads, _perf_nr_wordline_prog_fg,
      _perf_nr_wordline_prog_bg, _perf_nr_blk_erasures, _perf_nr_gc_trigger_bg,
      _perf_nr_discard_reqs, _perf_nr_discarded_lpgs);
  return 0;
}

static int fb_proc_open(__attribute__((unused)) struct inode *inode,
                        struct file *file) {
  return single_open(file, fb_proc_show, NULL);
}

// procfs를 통해, 커널 정보를 사용자 영역에서 접근 가능하다.
void perf_init(void) {
  // proc_create(): /proc 에다가 파일 생성하는 함수
  // 파일명, 권한, 디렉토리, file_operation 구조체
  struct proc_dir_entry *entry = proc_create("flashbench", 0444, NULL, &fb_proc_fops);

  if (!entry) {
    printk(KERN_ERR "flashbench: Failed to create /proc/flashbench\n");
    // TODO: Handle error properly
    // return -EEXIST;
  }
}

void perf_exit(void) {
  remove_proc_entry("flashbench", NULL);
}
