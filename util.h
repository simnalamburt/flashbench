#pragma once
#include <linux/types.h>

void perf_inc_nr_incomming_write(void);
void perf_inc_nr_wordline_prog_fg(void);
void perf_inc_nr_wordline_prog_bg(void);
void perf_inc_nr_page_reads(void);
void perf_inc_nr_blk_erasures(void);
void perf_inc_nr_gc_trigger_bg(void);
void perf_inc_nr_discard_reqs(void);
void perf_inc_nr_discard_lpgs(u32 nr_lpgs);

u32 timer_get_timestamp_in_us(void);

void perf_init(void);
void perf_exit(void);
void perf_display_result(void);
