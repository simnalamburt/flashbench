#pragma once
struct fb_gc_mngr_t;
struct fb_context_t;

struct fb_gc_mngr_t *create_gc_mngr(struct fb_context_t *fb);
void destroy_gc_mngr(struct fb_gc_mngr_t *gcm);
int trigger_gc_page_mapping(struct fb_context_t *ptr_fb_context);
void init_gcm(struct fb_gc_mngr_t *gcm);
int update_gc_blks(struct fb_context_t *fb);
int fb_bgc_prepare_act_blks(struct fb_context_t *fb);
int fb_bgc_set_vic_blks(struct fb_context_t *fb);
int fb_bgc_read_valid_pgs(struct fb_context_t *fb);
int prog_valid_pgs_to_gc_blks(struct fb_context_t *fb);
