#include <linux/slab.h>
#include <linux/mm.h>

#include "fb_bitmap.h"

#define BITMASK8(n) ( n==0 ? 0x80 : \
					( n==1 ? 0x40 : \
		  			( n==2 ? 0x20 : \
					( n==3 ? 0x10 : \
					( n==4 ? 0x08 : \
					( n==5 ? 0x04 : \
					( n==6 ? 0x02 : \
					( n==7 ? 0x01 : 0))))))))

#define BITMASK8_NEG(n) ( n==0 ? 0x7F : \
						( n==1 ? 0xBF : \
		  				( n==2 ? 0xDF : \
						( n==3 ? 0xEF : \
						( n==4 ? 0xF7 : \
						( n==5 ? 0xFB : \
						( n==6 ? 0xFD : \
						( n==7 ? 0xFE : 0))))))))

struct fb_bitmap_t *fb_create_bitmap(uint32_t num_entries)
{
	struct fb_bitmap_t *ptr_bitmap = NULL;

	uint32_t num_8bit = (num_entries + 7) >> 3;

	if((ptr_bitmap = (struct fb_bitmap_t *) kmalloc(sizeof(struct fb_bitmap_t), GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "fb_bitmap: Memory allocation for bitmap pointer failed.\n");
		goto PTR_ALLOC_FAIL;
	}

	if((ptr_bitmap->ptr_bits = (uint8_t *) kmalloc(sizeof(uint8_t) * num_8bit, GFP_ATOMIC)) == NULL)
	{
		printk(KERN_ERR "fb_bitmap: Memory allocation for bits failed\n");
		goto BIT_ALLOC_FAIL;
	}

	ptr_bitmap->num_entries = num_entries;

	clear_bitmap(ptr_bitmap);

	return ptr_bitmap;

BIT_ALLOC_FAIL:
	if(ptr_bitmap != NULL)
	{
		kfree(ptr_bitmap);
	}

PTR_ALLOC_FAIL:
	return NULL;
}

void fb_destroy_bitmap(struct fb_bitmap_t *ptr_bitmap)
{
	if(ptr_bitmap != NULL)
	{
		if(ptr_bitmap->ptr_bits != NULL)
		{
			kfree(ptr_bitmap->ptr_bits);
		}

		kfree(ptr_bitmap);
	}
}

int set_bitmap(struct fb_bitmap_t *ptr_bitmap, uint32_t idx, int value)
{
	uint8_t new_value = (value == 0) ? 0 : 0xFF;
	uint32_t bit_idx = idx >> 3;
	uint32_t offset = idx & 0x7;

	int ret_value = 0;

	if(ptr_bitmap->num_entries <= idx)
	{
		printk(KERN_ERR "fb_bitmap - set_bitmap: Out of range: %d >= %d (bound)\n",
				idx, ptr_bitmap->num_entries);
		ret_value = -1;
	}
	else
	{
		if(new_value == 0)
		{
			new_value = ptr_bitmap->ptr_bits[bit_idx] & BITMASK8_NEG(offset);
		}
		else
		{
			new_value = ptr_bitmap->ptr_bits[bit_idx] | BITMASK8(offset);
		}

		ptr_bitmap->ptr_bits[bit_idx] = new_value;
	}

	return ret_value;
}

int get_bitmap(struct fb_bitmap_t* ptr_bitmap, uint32_t idx)
{
	uint32_t bit_idx = idx >> 3;
	uint32_t offset = idx & 0x07;
	uint8_t value;

	int ret_value = 0;

	if(ptr_bitmap->num_entries <= idx)
	{
		printk(KERN_ERR "fb_bitmap - set_bitmap: Out of range: %d >= %d (bound)\n",
				idx, ptr_bitmap->num_entries);
		ret_value = -1;
	}
	else
	{
		value = ptr_bitmap->ptr_bits[bit_idx] & BITMASK8(offset);
		ret_value = (value == 0) ? 0 : 1;
	}

	return ret_value;
}

void clear_bitmap(struct fb_bitmap_t* ptr_bitmap)
{
	memset(ptr_bitmap->ptr_bits, 0, sizeof(uint8_t) * ((ptr_bitmap->num_entries + 7) >> 3));
}
