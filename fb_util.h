
#define MAX_SUMMARY_BUFFER 4096
u32 get_nr_plocks (void);
u32 get_nr_blocks (void);
void perf_inc_nr_plocks (void);
void perf_inc_nr_blocks (void);
void perf_inc_nr_incomming_write(void);
void perf_inc_nr_wordline_prog_fg(void);
void perf_inc_nr_wordline_prog_bg(void);
void perf_inc_nr_page_reads(void);
void perf_inc_nr_blk_erasures(void);
void perf_inc_nr_gc_trigger_fg(void);
void perf_inc_nr_gc_trigger_bg(void);
void perf_inc_nr_lsb_pg_backup (void);
void perf_inc_nr_discard_reqs (void);
void perf_inc_nr_discard_lpgs (u32 nr_lpgs);
u32 get_nr_incomming_write (void);
u32 get_nr_wordline_prog_fg (void);
u32 timer_get_timestamp_in_us(void);
void fb_file_log (char* filename, char* string);
struct file* file_open(const char* path, int flags, int rights);
struct file* file_open_read(const char* path);
void file_close(struct file* file);
int file_sync(struct file* file);
void perf_init (void);
int proc_read_summary(char *page, char **start, off_t off,  int count, int *eof, void *data);
void perf_exit(void);
void perf_display_result(void);
