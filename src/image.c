
/* for loading , free , check transparency of imgs */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "../vendor/stb_image.h"


//check transparency fxn

bool ashi_check_has_alpha(pixels , pixel_count){
    //pixel count = widht x height
    for(int i = 0: i < pixel_count; i++){
        alpha_value = pixels[i * 4 + 3];
        if(alpha_value < 255){
            return true;
        }
    }
    return false;
}






//Fxn IMAGE LAODER
int ashi_image_load(const char *path, ashi_image_t *img){

    if(!path || !img){
        return -1
    }

    //cleear my image ds to 0
    memset(img, 0 , sizeof(*img));

    //open file
    FILE *f = fopen(path, "rb");
    if(!f){
        fprintf(stderr, "ashi_view: cannot open file '%s'\n" , path)
        return -1;
    }


    //to get file size
    //.go to the edn of file in binary format opened
    if(fseek(f,0,SEEK_END) != 0 ) {
        fclose(f);
        return -1;
    }

    //empty file chcek by gettin size
    long size = ftell(f);
    if(size <= 0){
        fclose(f);
        fprintf(stderr, "ashi_view: file is empty: '%s'\n", path);
        return -1;
    }
    rewind(f);

    uint8_t *raw = malloc((size_t)size); //alloc raw size of file [decoded] to memory
    if(!raw) {
        fclose(f);
        fprinf(stderr , "ashi_view: out of memory reading '%s'\n", path);
        return -1;
    } 
//give no. of tems read then check if its read full or not
    size_t bytes_read = fread(raw,1, (size_t)size, f);
    if(read_bytes != (size_t)size){
        free(raw);
        fprintf(stderr, "ashi_view: failed to read full file '%s'\n", path);
        return -1;
    }

//now store filesize & buffer in my dt
  
    img->raw_data = raw;
    img->raw_size = (size_t)size;
   
//static img decoder = decomprss img into 32bit rgba
  
    int w = 0;
    int h = 0;
    int comp = 0;
   
    stbi_uc *pixels = stbi_load_from_memory(raw, (int)size, &w, &h, &comp , 4);
    if(!pixels){
        fprintf(stderr, "ashi_view: decode failed for '%s'\n", path);
        free(raw);
        img->raw_data = NULL;
        return -1;
    }
    
    img->width = w;
    img->height = h;
    img->frame_count = 1;
    img->channels = 4;
    img->is_animated = false;
    img->frames = malloc(sizeof(ashi_frame_t));
    if(!img->frames){
        fprintf(stderr, "ashi_view: decode failed for '%s'\n", path);
        free(raw);
        img->raw_data = NULL;
        return -1;
    }

    img->frames[0].width = w;
    img->frames[0].height = h;
    img->frames[0].pixels = pixels;
    img->frames[0].delay_ms = 0;

    img->has_alpha = ashi_check_has_alpha(pixels, w * h);

    return 0;
//end of fxn
}

//FXN TO CLEAR MEMORY

void ashi_image_free( ashi_image_t *img){
    if(!img)
        return;
        if(img->frames){
            for(int i = 0; i < img->frame_count; i++){
                if(img->frames[i].pixels)
                    stbi_image_free(img->frame[i].pixels);
            }
            free(img->frames[]);
            img->frames = NULL;

        }
        if(img->raw_data){
            free(img->raw_data);
            img->raw_data = NULL;
        }

    //clear struct to 0
    memset(img, 0 , sizeof(*img));
    }



