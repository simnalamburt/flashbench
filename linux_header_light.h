#pragma once
//
// Lightweight Linux header. It is designed to be included instead of the actual
// Linux header. Made for c2rust transpilation.
//

void *vmalloc(unsigned long size);
void vfree(const void *addr);
__attribute__((format(printf, 1, 2))) __attribute__((__cold__)) int printk(
    const char *fmt, ...);
void *memset(void *, int, unsigned long);
void *memcpy(void *, const void *, unsigned long);

#ifndef KERN_ERR
#define KERN_ERR "\0013"
#endif
