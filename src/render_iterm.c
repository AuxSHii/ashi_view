#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_iterm.h"
#include "base64.h"
#include "resample.h"
#include "terminal.h"
#include "../vendor/stb_image_write.h"

//fist encode the png in base64

static void iterm_send_encoded(const uint8_t *data, size_t len, int cols, int rows ){
	//required base64 size
	size_t b64_len = base64_encoded_size(len);
	//dst buffer for base64 encode
	char *b64 = malloc(b64_len + 1);

	//mem alloc error
	if(!b64) return;

	//encode png file into base64 into buffer
	base64_encode(data , len ,b64, b64_len + 1);
    fprintf(stdout, "\x1b]1337;File=inline=1;width=%d;height=%d;preserveAspectRatio=1:%s\x07\n",
            cols, rows, b64);
    fflush(stdout);

    free(b64);
   
}   



//mem callback function
typedef struct 
{
	uint8_t *data;
	size_t size;
	size_t cap;

} MemBuffer;

static void mem_write_callback(void *context, void *data, int size){
	if(size <= 0) return; //invalid size
	MemBuffer *buf = (MemBuffer *)context;
	if(buf->size + (size_t)size > buf->cap) {
		size_t new cap = buf->cap ? buf->cap * 2 : 16384;
		while(new_cap < buf->size + (size_t)size) {
			new_cap += 2;
		}
		uint8_t *new_data = realloc(buf->data, new_cap);
		if(!new_data) return; //mem alloc error
		buf->data = new_data;
		buf->cap = new_cap;
	}
	memcpy(buf->data + buf->size, data , (size_t)size);
	buf->size += (size_t)size; 
}

//encoding a frame to png[frame that we modify in a way for ex: background checkerborad etc]
static uint8_t *encode_frame_to_png(const uint8_t *pixels, int w, int h, size_t *out_len){
	MemBuffer buf = { .data = NULL, .size = 0, .cap = 0}; //init with 0
    if(!stbi_write_png_to_func(mem_write_callback, &buf, w, h,4, pixels , 0)){
    	free(buf.data);
    	*out_len = 0;
    	return NULL;
    }
    *out_len = buf.size;
    return buf.data;
}

//applying background
static uint8_t *apply_background(const uint8_t *src, int w, int h, const ashi_bg_color_t *bg)
{
    if (bg->mode == BG_NONE) return NULL;

    size_t total_pixels = (size_t)w * (size_t)h;
    uint8_t *out = malloc(total_pixels * 4);
    if (!out) return NULL;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = ((size_t)y * (size_t)w + (size_t)x) * 4;
            uint8_t a = src[idx + 3];

            if (a == 255) {
                out[idx + 0] = src[idx + 0];
                out[idx + 1] = src[idx + 1];
                out[idx + 2] = src[idx + 2];
                out[idx + 3] = 255;
                continue;
            }

            uint8_t bg_r, bg_g, bg_b;
            if (bg->mode == BG_CHECKER) {
                int checker = ((x >> 4) ^ (y >> 4)) & 1;
                bg_r = bg_g = bg_b = checker ? 0x38 : 0x24;
            } else {
                bg_r = bg->r;
                bg_g = bg->g;
                bg_b = bg->b;
            }

            uint32_t inv_a = 255 - a;
            out[idx + 0] = (uint8_t)(((uint32_t)src[idx + 0] * a + (uint32_t)bg_r * inv_a + 127) / 255);
            out[idx + 1] = (uint8_t)(((uint32_t)src[idx + 1] * a + (uint32_t)bg_g * inv_a + 127) / 255);
            out[idx + 2] = (uint8_t)(((uint32_t)src[idx + 2] * a + (uint32_t)bg_b * inv_a + 127) / 255);
            out[idx + 3] = 255;
        }
    }
    return out;
}


//rendering final

int ashi_render_iterm(const ashi_image_t *img, const ashi_config_t *cfg)
{
    if (!img || img->frame_count <= 0) {
        return -1;
    }

    int term_cols = 80, term_rows = 24;
    terminal_get_size(&term_cols, &term_rows);

    int max_cols = cfg->target_cols > 0 ? cfg->target_cols : term_cols;
    int max_rows = cfg->target_rows > 0 ? cfg->target_rows : (term_rows > 2 ? term_rows - 2 : term_rows);

    int cols = 0, rows = 0;
    ashi_calc_fit_dimensions(img->width, img->height, max_cols, max_rows, 2.0, &cols, &rows);

    if (cfg->clear_screen) {
        terminal_clear_screen();
    }

    //fast path: if no background blending requested, pass through original raw file
    if (cfg->bg.mode == BG_NONE && img->raw_data && img->raw_size > 0) {
        iterm_send_encoded(img->raw_data, img->raw_size, cols, rows);
        return 0;
    }

    // single frame with background modification 
    if (!img->is_animated || img->frame_count == 1) {
        const uint8_t *px = img->frames[0].pixels;
        uint8_t *blended = apply_background(px, img->width, img->height, &cfg->bg);

        size_t png_len = 0;
        uint8_t *png = encode_frame_to_png(blended ? blended : px, img->width, img->height, &png_len);
        if (png) {
            iterm_send_encoded(png, png_len, cols, rows);
            free(png);
        }
        free(blended);
        return 0;
    }