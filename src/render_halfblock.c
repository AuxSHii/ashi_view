
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
    //char data ->str = source = color , unicde blokc code etc
    
    if(b->len + n >= sizeof(b->data)){
        outbuf_flush(b);
    }
    memcpy(b->data + b-> len , str , n); 
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

static inline void output_fb_rgb(OutBuf *b uint8_t r, uint8_t g , uint8_t b_col){
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






