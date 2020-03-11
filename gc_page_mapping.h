struct fb_gc_mngr_t;
struct fb_context_t;

struct fb_gc_mngr_t *create_gc_mngr (struct fb_context_t *fb);
void destroy_gc_mngr (struct fb_gc_mngr_t *gcm);
int trigger_gc_page_mapping(struct fb_context_t *ptr_fb_context);
int trigger_bg_gc (struct fb_context_t *fb);
