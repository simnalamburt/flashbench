// Lock structure
#define fb_lock_t completion

#define fb_init_lock(a) 				\
do {							\
	init_completion (a);		\
	complete (a);				\
} while (0)

#define fb_lock(a) 				\
do {							\
	wait_for_completion (a);	\
	reinit_completion (a);		\
} while (0)

#define fb_unlock(a) 			\
do {							\
	complete (a);				\
} while (0)

// Print Func
#define fb_print_inf(fmt, ...)							\
do { 													\
	printk(KERN_INFO "[FB_INF]: " fmt,  ##__VA_ARGS__);	\
} while (0)

#define fb_print_err(fmt, ...)									\
do { 													\
	printk(KERN_INFO "[FB_ERR]: " fmt,  ##__VA_ARGS__);	\
} while (0)

//Memory
#define fb_malloc(a) vmalloc(a)
