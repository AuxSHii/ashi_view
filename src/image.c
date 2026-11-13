
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






//Fxn IMAGE LAODER - it fills our image container struct.
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
    img->channels = 4;


//GIF decoding 
    //1.detect gif format by magic signature

    bool is_gif = (size >= 6 && memcmp(raw, "GIF87a", 6) == 0 || memcmp(raw, "GIF89a", 6) == 0);

   //if its a gif
    if(is_gif){
        int *delays = NULL; //ptr to temp var to store delay [int array]
               //eventualy [stbi_load_gif_from_memory] will store all frame's delays values here [as an array 0 , 1, 2 ,..... so on] 
        int w = 0, h = 0, frames = 0; , comp = 0; //gif vars

        //stbi contigious  chunk of pixels
        stbi_uc *gif_pixels = stbi_load_gif_from_memory(raw, (int)size, &delays, &w, &h, &frames, &comp, 4);

        //give img container its static values
        if(gif_pixels && frames > 0){
            img->width = w;
            img->height = h;
            img->frame_count = frames;
            img->is_animated = (frames > 1); //check right here
            //alloc memory for entire frames arr + point it to mem loc
            img->frames = malloc((size_t)frames * sizeof(ashi_frame_t));
            if(!img->frames){ //if mem alloc failed
                stbi_image_free(gif_pixels);
                free(delays);
                free(raw);
                return -1;
            }

            //calc size needed for 1 frame [pixels size {r,g,b,a} thoughout the image for every pixel] so we can use it as mem offset to jump from one frame to another
            size_t frame_byte_size = (size_t)w * (size_t)h * 4;
            //init apha state cheker var
            bool found_alpha = false;

            /*loop to slice 1 frame from stbi given chunk[gif_pixels] 
            and put it in its individual[ashi_frame_t = pixels,delay_ms, h,w]
            so every frame is identical to an img 
             */

            for(int i = 0; i < frames; i++){
                img->frames[i].width = w; //for ith frame 
                img->frames[i].height = h;
                //allocate mem for {r,g,b,a}=pixel arr of ith frame
                img->frames[i].pixels = malloc(frame_byte_size);
                //handle partial memmory allocation failure
                  //actually, remove or free every memory allocated before the mem alloc failure of ith frame
                  //though rollback failure pattern

                //free everything upto i 
                for(int j = 0; j < i; j++){
                    free(img->frames);
                    stbi_image_free(gif_pixels); //free the chonk
                    free(delays);
                    free(raw);
                    return -1;
                }

                //if everything is fine then copy the pixels of every frame from choonly 
                 //to the indiv frames ith's allocated mem location

                memcpy(img->frames[i].pixels, gif_pixels + (size_t)i * frame_byte_size, frame_byte_size);
                
                //update other vars [delay,and alpha checker]
                int d = delays ? delays[i] : 100; //delay of ith frame , if no amt given then take 100

                //standard browser gif heuristic : clamp <=10ms delays to 100ms
                if(d < 10){
                    d = 100;
                }

                //put delay val in img container's ith frames
                img->frames[i].delay_ms = d;

                //cehck if ith frame has alpha
                if(!found_alpha && ashi_check_has_alpha(img->frames[i].pixels, w* h)){
                    found_alpha = true;
                }
            }
                //update has_alpha in img conters ith frame
                img->has_alpha = found_alpha;
                
                //free chonk + delays
                stbi_image_free(gif_pixels);
                free(delays); //our temp delay array
                return 0;
            }
        }

    



//static img decoder = decomprss img into 4bytes rgba
  
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



