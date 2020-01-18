
struct fb_bitmap_t
{
	uint32_t num_entries;

	uint8_t *ptr_bits;
};

struct fb_bitmap_t *fb_create_bitmap(uint32_t num_entries);
void fb_destroy_bitmap(struct fb_bitmap_t *ptr_bitmap);
int set_bitmap(struct fb_bitmap_t* ptr_bitmap, uint32_t idx, int value);
int get_bitmap(struct fb_bitmap_t* ptr_bitmap, uint32_t idx);
void clear_bitmap(struct fb_bitmap_t* ptr_bitmap);
