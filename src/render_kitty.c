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


