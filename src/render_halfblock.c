
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
