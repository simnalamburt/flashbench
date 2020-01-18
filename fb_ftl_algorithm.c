#include "fb.h"
#include "fb_option.h"

#if (FTL_ALGORITHM==PAGE_LEVEL_FTL)
#include "fb_ftl_algorithm_page_mapping.h"
#endif

void *create_mapping_context(struct fb_context_t *ptr_fb_context)
{

#if (FTL_ALGORITHM==PAGE_LEVEL_FTL)
	return create_pg_ftl (ptr_fb_context);
#endif
}

void destroy_mapping_context(struct fb_context_t *ptr_fb_context)
{
#if (FTL_ALGORITHM==PAGE_LEVEL_FTL)
	destroy_pg_ftl ((fb_pg_ftl_t *) get_ftl (ptr_fb_context));
#endif
}
