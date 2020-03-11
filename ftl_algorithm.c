#include "fb.h"
#include "ftl_algorithm_page_mapping.h"

void *create_mapping_context(struct fb_context_t *ptr_fb_context) {
	return create_pg_ftl (ptr_fb_context);
}

void destroy_mapping_context(struct fb_context_t *ptr_fb_context) {
	destroy_pg_ftl ((struct page_mapping_context_t *) get_ftl (ptr_fb_context));
}
