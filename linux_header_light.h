#pragma once
//
// Lightweight Linux header. It is designed to be included instead of the actual
// Linux header. Made for c2rust transpilation.
//
#include <linux/types.h>

// linux/printk.h
__attribute__((format(printf, 1, 2))) __attribute__((__cold__)) int printk(
    const char *fmt, ...);
#ifndef KERN_ERR
// NOTE: 이 매크로는 c2rust에 의해 못생기게 트랜스파일되는것을 감수하고 *_ex.h 에 두지 않기로했음
#define KERN_ERR "\0013"
#endif

// linux/string.h
void *memset(void *, int, unsigned long);
void *memcpy(void *, const void *, unsigned long);

// linux/vmalloc.h
void *vmalloc(unsigned long size);
void vfree(const void *addr);

// linux/bio.h
struct bio;
void bio_endio(struct bio *);

// linux/seq_file.h
struct file;
struct seq_file;
ssize_t seq_read(struct file *, char __user *, size_t, loff_t *);
void seq_printf(struct seq_file *m, const char *fmt, ...);
int single_open(struct file *, int (*)(struct seq_file *, void *), void *);
