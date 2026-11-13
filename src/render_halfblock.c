
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "render_halfblock.h"
#include "resample.h"
#include "terminal.h"

//64kb bulk i/o buffer

typedef struct {
    char data[65536];
    size_t len;
} OutBuf;


fxn takes buffer and flush it out to terminal

static inline void outbuf_flush(OutBuf *b){
    if(b->len > 0){
        fwrite(b->data , 1 , b->len, stdout); //flush the buff out
        b->len = 0; //afer finished 
    }
}

//fxn to append buffer
static inline void outbuf_append(OutBuf *b , const char *str , size_t n){
    //char data ->str = source => color cmd ---append_in--> 64kb buffer OutBuf , 
    
    if(b->len + n >= sizeof(b->data)){
        outbuf_flush(b);
    }
    memcpy(b->data + b->len , str , n); 
    b->len += n; //update length of buffer
}
//str appender 

static inline void outbuf_append_str(OutBuf *b , const char *str){
    outbuf_append(b,str,strlen(str));
}

static inline void outbuf_fg_rgb(OutBuf *b  uint8_t r, uint8_t g , uint8_t b_col){
    //temp string
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "\x1b[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b_col);
        outbuf_append(b, tmp, (size_t)n);
}

static inline void output_bg_rgb(OutBuf *b uint8_t r, uint8_t g , uint8_t b_col){
    char tmp[32];
        int n = snprintf(tmp, sizeof(tmp), "\x1b[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b_col);
        outbuf_append(b, tmp, (size_t)n);
}

static inline blend_pixel(const uint8_t *p, int x , int y , const ashi_bg_color_t *bg,
                            uint8_t *out_r, uint8_t *out_g , uint8_t *out_b, bool *out_is_trans){
        
        //handle the case of complete transparent img
        uint8_t a = p[3]; //alpha val
        if(bg->mode == BG_NONE && a < 32){
            *out_is_trans = true;
            *out_r = *out_g = *out_b = 0; //o/p is transparent
            return;
            //cuz fxn is void: i am modifing vals in place through deref
        }
        
        *out_is_trans = false; //else o/p is not transparent

        //when its opaque , give rgb its corsponding vals from pixel arr
        if(a == 255){
            *out_r = p[0];
            *out_g = p[1];
            *out_b = p[2];
            return;
        }

//ALPHA BLENDING [if  0 < alpha < 255] => checkerboard
        /* 
          bg rgb vals <- blend mode acc to vals 

        */
        uint8_t bg_r , bg_g , bg_b; //blended rgb vals
        if(bg->mode == BG_CHECKER) {
            int checker = ((x >> 1) ^ (y >> 1)) & 1; //chck with xor
            bg_r = bg_g = bg_b = checker ? 0x38 : 0x24;
        }
        elseif(bg->mode == BG_SOLID){
            bg_r = bg->r;
            bg_g = bg->g;  //give rgb vals as it is if solid bg
            bg_b = bg->b;
        }
        else {
            bg_r = bg_g = bg_b = 0;
        }

        uint32_t inv_a = 255 -a;
        *out_r = (uint8_t)(((uint32_t)p[0] * a + (uint32_t)bg_r * inv_a + 127) / 255);
        *out_g = (uint8_t)(((uint32_t)p[1] * a + (uint32_t)bg_g * inv_a + 127) / 255);
        *out_b = (uint8_t)(((uint32_t)p[2] * a + (uint32_t)bg_b * inv_a + 127) / 255);

}



static void render_frame_halfblock(const uint8_t *pixels, int cols , int rows ,
                                   const ashi_bg_color_t *bg , OutBuf *buf){
    for(int r = 0; r < rows; r++){
        int y_top = r * 2;
        int y_bot = r * 2 + 1;

        int cur_fg_r = -1, cur_fg_g = -1, cur_fg_b = -1;
        int cur_bg_r = -1; cur_bg_g = -1, cur_bg_b = -1;
        bool cur_fg_default = false;
        bool cur_bg_default = false;

        //loop for colums [left->right]
        for(int c = 0; c < cols; c++){
        const uint8_t *p_top = &pixels[(y_top * cols + c) * 4];
        const uint8_t *p_bot = &pixels[(y_bot * cols + c) * 4];

        uint8_t tr, tg, tb, br, bg_col, bb; //var for rgb {top+bott}
        bool top_trans, bot_trans; //bool for are they transparent?
        
        blend_pixel(p_top, c, y_top, bg, &tr, &tg, &tb, &top_trans);
        blend_pixel(p_bot, c, y_bot, bg, &br, &bg_col, bb ,&bot_trans);

        //blend pixel will edit these vars 
        if(top_trans && bot_trans){
            if(!cur_bg_default || !cur_fg_default){
                outbuf_append_str(buf, "\x1b[0m");
                cur_fg_r = cur_fg_g = cur_fg_b = -1; //state var still -1 if img-transparent
                cur_bg_r = cur_bg_g = cur_bg_b = -1;
                cur_fg_default = true;
                cur_bg_default = true;
            }

            outbuf_append(buf, " ", 1);
        }
        //case ; IF top is transparent
        else if(top_trans){
            //only bottom is colored; emit lower half block
            if(cur_fg_r != bg_r || cur_fg_g != bg_col || cur_fg_g != bg_b || cur_fg_default){
                outbuf_fg_rgb(buf, bg_r, bg_col, bg_b);
                //update state [current] fg color var
                cur_fg_r = bg_r; cur_fg_g = bg_col; cur_fg_b = bg_b;
                cur_fg_default = false;
            }
            if(!cur_bg_default){
                //terminal should reset to default color
                outbuf_append_str(buf, "\x1b[49m");
                
                cur_bg_r = cur_bg_g = cur_bg_b = -1;
                cur_bg_default = true;
            }
                                  //any other case
            outbuf_append_str(buf, "\xe2\x96\x84"); //printlower block char  ▄
        }

        //case: if bottom is transparent
        else if(bot_trans){
            if(cur_fg_r != tr || cur_fg_g != tg || cur_fg_b != tb || cur_fg_default){
                outbuf_fg_rgb(buf, tr, tg, tb);
                //update state variables
                cur_fg_r = tr; cur_fg_g = tg; cur_fg_b = tb;
                cur_fg_default = false;
            }
            //if current state bg colors have some vals 
            if(!cur_bg_default){
                //reset termnl color
                outbuf_append_str(buf, "\x1b[49m");
                //reset state vars
                cur_bg_r = cur_bg_g = cur_bg_b = -1;
                cur_bg_default = true;
            }

            outbuf_append_str(buf, "\xe2\x96\x80"); //print upper-half-block  ▀ 
            }

            else {
                //nothing is transparent
                //1. color fg = top = upper-half-block
                if(cur_fg_r != tr || cur_fg_g != tg || cur_fg_b != tg || cur_fg_default){
                    //color fg
                    outbuf_fg_rgb(buf, tr, tg, tb);
                    //update state fg vars
                    cur_fg_r = tr; cur_fg_g = tg; cur_fg_b = tb;
                    cur_fg_default = false;
                }

                //2. color bg = no unicode char here = terminal char cell
                if(cur_bg_r != br || cur_bg_g != bg_col || cur_bg_b = bb || cur_bg_default)
                    //color bg
                        outbuf_bg_rgb(buf, br,bg_col, bb);
                    //update state bg vars & bg bool
                        cur_bg_r = br; cur_bg_g = bg_col; cur_fg_b = bb;
                        cur_bg_default = false;
            }
        }
            //print 1 unicode upper half block ▀ for that 1 iteration of character cell
                 outbuf_append_str(buf, "\xe2\x96\x80");       
    }

        //completed for every char cell[row and cols]
        //flush teh buffer out
        outbuf_flush(buf);

//end of fxn    
}

//rendering lifetime manager fxn 
//takes img + config and renders img into termninal through unicde mechanism wrote above

int ashi_render_halfblock(const ashi_image_t *img, const ashi_config_t *cfg){
    //invalid input case
    if(!img || img->frame_count <= 0){
        return -1;
    }

    int term_cols = 80, term_rows = 24; //dreclared and intlzd rows and cols var
    //query terminl size
    terminal_get_size(&term_cols , &term_rows); 
   //update max_cols and rows 
    int max_cols = cfg->target_cols > 0 ? cfg->target_cols : term_cols;
    int max_rows = cfg->target_rows > 0 ? cfg->target_rows : (term_rows > 2 ? term_rows - 2 : term_rows);

    // intilzd var r,c for fitting dimension output 
    int rows =0, cols = 0;
   //aspect ratio = 2.0; as each 1 cell contains <= 2 vertical subpixels
   ashi_calc_fit_dimensions(img->width, img->height,max_cols, max_rows , 2.0, &cols , &rows ) 
   
   int subpixels_h = rows * 2;

   //allocate frame buffer of corrsponding size
   size_t frame_bytes = (size_t)cols * (size_t)subpixel_h * 4;

    // pre-resample all frames for zero-overhead animation playback 
    uint8_t **scaled_frames = malloc((size_t)img->frame_count * sizeof(uint8_t *));
    if (!scaled_frames) {
        return -1;
    }

    for (int f = 0; f < img->frame_count; ++f) {
        scaled_frames[f] = malloc(frame_bytes);
        if (!scaled_frames[f]) {
            for (int j = 0; j < f; ++j) free(scaled_frames[j]);
            free(scaled_frames);
            return -1;
        }
        ashi_resample_rgba(img->frames[f].pixels, img->width, img->height,
                           scaled_frames[f], cols, subpixel_h);
    }
    
   OutBuf buf = { .len = 0 }; //initialized buffer with length of 0
   
   if(cfg->clear_screen){
    terminal_clear_screen();
   } 

   terminal_hide_cursor();
   
   //if static img -> render once and exit
   if(!img->is_animated || img->frame_count == 1){
       //render img
        render_frame_halfblock(scaled_frames[0], cols ,rows, &cfg->bg , &buf);
        terminal_show_cursor();
        terminal_reset_color(); 
        fflush(stdout);

        free(scaled_frames[0]);
        free(scaled_frames);
        return 0;
   }


   //animation playback loop[will define later]
    // animation playback loop 
    bool infinite = (cfg->loop_count < 0 || cfg->loop_count == 0);
    int max_loops = infinite ? -1 : cfg->loop_count;
    int current_loop = 0;

    while (!g_terminal_interrupted && (max_loops < 0 || current_loop < max_loops)) {
        for (int f = 0; f < img->frame_count; ++f) {
            uint64_t t_start = terminal_time_ms();

            if (current_loop > 0 || f > 0) {
                terminal_cursor_up(rows);
            }

            render_frame_halfblock(scaled_frames[f], cols, rows, &cfg->bg, &buf);

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
    terminal_reset_color();
    fflush(stdout);

    for (int f = 0; f < img->frame_count; ++f) {
        free(scaled_frames[f]);
    }
    free(scaled_frames);

    return 0;
//end of fxn
}













