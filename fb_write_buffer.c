#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include "uthash.h"
#include "utlist.h"
#include "fb_option.h"
#include "fb_macro.h"
#include "fb_write_buffer.h"

static inline void fb_wb_set_pg_lpa (struct fb_wb_pg_t *wb_pg, uint32_t lpa) {
	wb_pg->lpa = lpa;
}

static inline uint32_t fb_wb_get_pg_lpa (struct fb_wb_pg_t *wb_pg) {
	return wb_pg->lpa;
}

static inline void fb_wb_set_pg_wflag (struct fb_wb_pg_t *wb_pg, int flag) {
	wb_pg->wflag = flag;
}

static inline int fb_wb_get_pg_wflag (struct fb_wb_pg_t *wb_pg) {
	return wb_pg->wflag;
}

static void fb_destroy_wb_entry (struct fb_wb_pg_t *wb_pg) {
	if (wb_pg != NULL) {
		if (wb_pg->data != NULL)
			vfree (wb_pg->data);

		vfree (wb_pg);
	}
}

static struct fb_wb_pg_t *fb_create_wb_entry (uint32_t pg_size) {
	struct fb_wb_pg_t *wb_pg;

	if ((wb_pg = (struct fb_wb_pg_t *) vmalloc (sizeof (struct fb_wb_pg_t))) == NULL) {
		printk(KERN_ERR "fb_write_buffer: Allocating buffered page failed.\n");
		goto FAIL;
	}

	if ((wb_pg->data = (uint8_t *) vmalloc (sizeof (uint8_t) * pg_size)) == NULL) {
		printk(KERN_ERR "fb_write_buffer: Allocating data buffer failed.\n");
		goto FAIL;

	}

	fb_wb_set_pg_lpa (wb_pg, -1);
	fb_wb_set_pg_wflag (wb_pg, FALSE);

	return wb_pg;

FAIL:
	fb_destroy_wb_entry (wb_pg);

	return NULL;
}

static inline void fb_wb_set_free_pg (struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
	DL_APPEND (wb->free_pgs, wb_pg);
}

static inline struct fb_wb_pg_t *fb_wb_get_free_pg (struct fb_wb *wb) {
	return wb->free_pgs;
}

static inline void fb_wb_reset_free_pg (struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
	DL_DELETE (wb->free_pgs, wb_pg);
}

static inline void fb_wb_set_writing_pg (struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
	DL_APPEND (wb->writing_pgs, wb_pg);
	fb_wb_set_pg_wflag (wb_pg, TRUE);
}

static inline struct fb_wb_pg_t *fb_wb_get_writing_pg (struct fb_wb *wb) {
	return wb->writing_pgs;
}

static inline void fb_wb_reset_writing_pg (struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
	DL_DELETE (wb->writing_pgs, wb_pg);
	fb_wb_set_pg_wflag (wb_pg, FALSE);
}

static inline uint32_t fb_wb_set_buf_pg (struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
	DL_APPEND (wb->buf_pgs, wb_pg);
	return ++(wb->nr_entries);
}

static inline struct fb_wb_pg_t *fb_wb_get_buf_pg_head (struct fb_wb *wb) {
	return wb->buf_pgs;
}

static inline struct fb_wb_pg_t *fb_wb_get_buf_pg (struct fb_wb *wb, uint32_t lpa) {
	struct fb_wb_pg_t *wb_pg = NULL;

	HASH_FIND (hh, wb->hash_pgs, &lpa, sizeof (uint32_t), wb_pg);

	return wb_pg;
}

static inline uint32_t fb_wb_reset_buf_pg (struct fb_wb *wb, struct fb_wb_pg_t *wb_pg) {
	DL_DELETE (wb->buf_pgs, wb_pg);
	return --(wb->nr_entries);
}

struct fb_wb *fb_create_write_buffer (uint32_t nr_max_entries, uint32_t pg_size) {
	struct fb_wb *wb;
	struct fb_wb_pg_t *wb_pg;
	uint32_t i;

	if ((wb = (struct fb_wb *) vmalloc (sizeof (struct fb_wb))) == NULL) {
		printk (KERN_ERR "fb_write_buffer: Allocating write buffer failed.\n");
		goto FAIL;
	}

	wb->pg_size = pg_size;
	wb->nr_entries = 0;

	wb->free_pgs = NULL;
	wb->writing_pgs = NULL;
	wb->buf_pgs = NULL;

	wb->hash_pgs = NULL;

	for (i = 0 ; i < nr_max_entries ; i++) {
		if ((wb_pg = fb_create_wb_entry (pg_size)) == NULL) {
			printk (KERN_ERR "fb_write_buffer: Creating wb entry failed.\n");
			goto FAIL;
		}

		fb_wb_set_free_pg (wb, wb_pg);
	}

	if ((fb_wb_get_pg_buf (wb) = (uint8_t *) fb_malloc (
					sizeof (uint8_t) * PHYSICAL_PAGE_SIZE)) == NULL) {
		fb_print_err ("WB - Allocating page buffer failed.\n");
		goto FAIL;
	}

	init_completion (&wb->wb_lock);
	complete (&wb->wb_lock);
	//spin_lock_init (&wb->wb_lock);

	return wb;

FAIL:
	fb_destroy_write_buffer (wb);

	return NULL;
}

void fb_destroy_write_buffer (struct fb_wb *wb) {
	struct fb_wb_pg_t *wb_pg;

	if (wb != NULL) {
		if (wb->free_pgs != NULL) {
			while ((wb_pg = fb_wb_get_free_pg (wb)) != NULL) {
				fb_wb_reset_free_pg (wb, wb_pg);
				fb_destroy_wb_entry (wb_pg);
			}
		}

		if (wb->buf_pgs != NULL) {
			while ((wb_pg = fb_wb_get_buf_pg_head (wb)) != NULL) {
				fb_wb_reset_buf_pg (wb, wb_pg);
				HASH_DEL (wb->hash_pgs, wb_pg);
				fb_destroy_wb_entry (wb_pg);
			}
		}

		if (wb->writing_pgs != NULL) {
			while ((wb_pg = fb_wb_get_writing_pg (wb)) != NULL) {
				fb_wb_reset_writing_pg (wb, wb_pg);
				HASH_DEL (wb->hash_pgs, wb_pg);
				fb_destroy_wb_entry (wb_pg);
			}
		}

		vfree (wb);
	}
}

inline uint32_t fb_get_nr_pgs_in_wb (struct fb_wb *wb, int lock) {
	uint32_t ret;

	if (lock == TRUE) {
		//spin_lock_irqsave (&wb->wb_lock, flag);
		wait_for_completion (&wb->wb_lock);
		reinit_completion (&wb->wb_lock);
	}

	ret = wb->nr_entries;

	if (lock == TRUE)
		complete (&wb->wb_lock);
		//spin_unlock_irqrestore (&wb->wb_lock, flag);

	return ret;
}

int fb_get_pg_data (struct fb_wb *wb, uint32_t lpa, uint8_t* dest) {
	struct fb_wb_pg_t *wb_pg;
	int ret = -1;

	//spin_lock_irqsave (&wb->wb_lock, flag);
	wait_for_completion (&wb->wb_lock);
	reinit_completion (&wb->wb_lock);

	if ((wb_pg = fb_wb_get_buf_pg (wb, lpa)) != NULL) {
		memcpy (dest, wb_pg->data, wb->pg_size);
		ret = 0;
	}

	//spin_unlock_irqrestore (&wb->wb_lock, flag);
	complete (&wb->wb_lock);

	return ret;
}

int fb_get_pgs_to_write (struct fb_wb *wb, uint32_t nr_pgs, uint32_t *lpas, uint8_t *dest) {

	struct fb_wb_pg_t *wb_pg;
	int ret = -1;
	uint32_t i;

	wait_for_completion (&wb->wb_lock);
	reinit_completion (&wb->wb_lock);
	//spin_lock_irqsave (&wb->wb_lock, flag);

	if (fb_get_nr_pgs_in_wb (wb, FALSE) >= nr_pgs) {
		for (i = 0 ; i < nr_pgs ; i++) {
			wb_pg = fb_wb_get_buf_pg_head (wb);
			fb_wb_reset_buf_pg (wb, wb_pg);
			fb_wb_set_writing_pg (wb, wb_pg);

			memcpy (dest + (i * wb->pg_size), wb_pg->data, wb->pg_size);
			lpas[i] = fb_wb_get_pg_lpa (wb_pg);
		}

		ret = 0;
	}

	//spin_unlock_irqrestore (&wb->wb_lock, flag);
	complete (&wb->wb_lock);

	return ret;
}

int fb_put_pg (struct fb_wb *wb, uint32_t lpa, uint8_t* src) {
	struct fb_wb_pg_t *wb_pg;
	int ret = -1;

	if ((wb_pg = fb_wb_get_buf_pg (wb, lpa)) == NULL) {
		if ((wb_pg = fb_wb_get_free_pg (wb)) == NULL)
			goto FINISH;

		fb_wb_reset_free_pg (wb, wb_pg);

		fb_wb_set_pg_lpa (wb_pg, lpa);
		HASH_ADD (hh, wb->hash_pgs, lpa, sizeof (uint32_t), wb_pg);
	} else {
		if (fb_wb_get_pg_wflag (wb_pg) == TRUE)
			fb_wb_reset_writing_pg (wb, wb_pg);
		else
			fb_wb_reset_buf_pg (wb, wb_pg);
	}

	memcpy (wb_pg->data, src, wb->pg_size);
	fb_wb_set_buf_pg (wb, wb_pg);

	ret = 0;

FINISH:

	return ret;
}

void fb_rm_written_pgs (struct fb_wb *wb) {
	struct fb_wb_pg_t *wb_pg;

	while ((wb_pg = fb_wb_get_writing_pg (wb)) != NULL) {
		fb_wb_reset_writing_pg (wb, wb_pg);
		HASH_DEL (wb->hash_pgs, wb_pg);

		fb_wb_set_free_pg (wb, wb_pg);
	}
}

void fb_rm_buf_pg (struct fb_wb *wb, uint32_t lpa) {
	struct fb_wb_pg_t *wb_pg;

	if ((wb_pg = fb_wb_get_buf_pg (wb, lpa)) != NULL) {
		if (fb_wb_get_pg_wflag (wb_pg) == TRUE)
			fb_wb_reset_writing_pg (wb, wb_pg);
		else
			fb_wb_reset_buf_pg (wb, wb_pg);
		HASH_DEL (wb->hash_pgs, wb_pg);

		fb_wb_set_free_pg (wb, wb_pg);
	}

}
