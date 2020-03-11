
struct fb_bitmap_t
{
	u32 num_entries;

	u8 *ptr_bits;
};

struct fb_bitmap_t *fb_create_bitmap(u32 num_entries);
void fb_destroy_bitmap(struct fb_bitmap_t *ptr_bitmap);
int set_bitmap(struct fb_bitmap_t* ptr_bitmap, u32 idx, int value);
int get_bitmap(struct fb_bitmap_t* ptr_bitmap, u32 idx);
void clear_bitmap(struct fb_bitmap_t* ptr_bitmap);
