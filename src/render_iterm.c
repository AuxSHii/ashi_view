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