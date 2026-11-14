#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_kitty.h"
#include "base64.h"
#include "terminal.c"
#include "resample.c"

//max raw bytes per chunk = 3072 bytes --> exactly 4096bytes base64 chars (kitty max chunk size) 
#define KITTY_RAW_CHUNK_SIZE 3072

static void kitty_send_rgba(const uint8_t *rgba, int w, int h, int cols, int rows){
	size_t offset = 0;
	size_t total_bytes = (size_t)w * (size_t)h * 4;
    bool is_first = true;
    char b64_chunk[4096 + 1];
    /*
          all this fxn does: is take image pixel data encode
          it in base64 then takes base64 data in chunks [size limited by kitty]
          and starts sending them to stdout buffer with their appropriate
          apc escape command [sendng the base64 chunk data as payload to display ]
          then flush the stdout buffer OUT!! 
     */

    //start slicing
    while(offset < total_bytes){
    	//we have to break it
    	size_t chunk_raw = total_bytes - offset; 
    	if(chunk_raw > KITTY_RAW_CHUNK_SIZE){
    		chunk_raw = KITTY_RAW_CHUNK_SIZE;
    	}

    	//now append the chunk encode into base64 & append to dst buffer
    	 base64_encode(rgba + offset, chunk_raw , b64_chunk , sizeof(b64_chunk));
    	 //increment in offset by 1chunk
    	 offset += chunk_raw;
    	 bool is_last = (offset >= total_bytes);

    	 if(is_first){
            fprintf(stdout, "\x1b_Ga=T,f=32,s=%d,v=%d,c=%d,r=%d,m=%d;%s\x1b\\",
                    w, h, cols, rows, is_last ? 0 : 1, b64_chunk);
            is_first = false; //have put fist out
    	 }
    	 else {
             fprintf(stdout, "\x1b_Gm=%d;%s\x1b\\", is_last ? 0 : 1, b64_chunk);
    	}
    }

    fputs("\n", stdout);
    fflush(stdout);
}

static uint8_t *apply_background(const uint8_t *src, int w, int h, const ashi_bg_color_t *bg)
{
    if (bg->mode == BG_NONE) {
        return NULL;
    }

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
                // 16x16 pixel checkerboard pattern
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

int ashi_render_kitty(const ashi_image_t *img, const ashi_config_t *cfg)
{
    if (!img || img->frame_count <= 0) {
        return -1;
    }

    int term_cols = 80, term_rows = 24;
    terminal_get_size(&term_cols, &term_rows);

    int max_cols = cfg->target_cols > 0 ? cfg->target_cols : term_cols;
    int max_rows = cfg->target_rows > 0 ? cfg->target_rows : (term_rows > 2 ? term_rows - 2 : term_rows);

    int cols = 0, rows = 0;
    // aspect ratio = 1.0 (terminal cells are approximately 1:2, so 2 cell height per width) 
    ashi_calc_fit_dimensions(img->width, img->height, max_cols, max_rows, 2.0, &cols, &rows);

    if (cfg->clear_screen) {
        terminal_clear_screen();
    }

    terminal_hide_cursor();

    // single static image 
    if (!img->is_animated || img->frame_count == 1) {
        const uint8_t *pixels = img->frames[0].pixels;
        uint8_t *blended = apply_background(pixels, img->width, img->height, &cfg->bg);
        kitty_send_rgba(blended ? blended : pixels, img->width, img->height, cols, rows);
        free(blended);

        terminal_show_cursor();
        return 0;
    }

    // animated GIF playback 
    bool infinite = (cfg->loop_count < 0 || cfg->loop_count == 0);
    int max_loops = infinite ? -1 : cfg->loop_count;
    int current_loop = 0;

    // pre-blend frames if custom background was chosen 
    uint8_t **frame_buffers = malloc((size_t)img->frame_count * sizeof(uint8_t *));
    for (int f = 0; f < img->frame_count; ++f) {
        uint8_t *b = apply_background(img->frames[f].pixels, img->width, img->height, &cfg->bg);
        frame_buffers[f] = b ? b : img->frames[f].pixels;
    }

    while (!g_terminal_interrupted && (max_loops < 0 || current_loop < max_loops)) {
        for (int f = 0; f < img->frame_count; ++f) {
            uint64_t t_start = terminal_time_ms();

            if (current_loop > 0 || f > 0) {
                terminal_cursor_up(rows + 1);
            }

            kitty_send_rgba(frame_buffers[f], img->width, img->height, cols, rows);

            float speed = cfg->speed > 0.05f ? cfg->speed : 1.0f;
            int delay = (int)((float)img->frames[f].delay_ms / speed);
            if (delay < 10) delay = 10;

            uint64_t elapsed = terminal_time_ms() - t_start;
            if (elapsed < (uint64_t)delay) {
                terminal_sleep_ms((uint32_t)((uint64_t)delay - elapsed));
            }

            char key = 0;
            if (terminal_check_key(&key)) {
                if (key == 'q' || key == 'Q' || key == 27) {
                    g_terminal_interrupted = true;
                    break;
                }
            }
            if (g_terminal_interrupted) break;
        }
        current_loop++;
    }

    terminal_show_cursor();
    fflush(stdout);

    if (cfg->bg.mode != BG_NONE) {
        for (int f = 0; f < img->frame_count; ++f) {
            free(frame_buffers[f]);
        }
    }
    free(frame_buffers);

    return 0;
}



