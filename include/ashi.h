#ifndef ASHI_H
#define ASHI_H

#include <stdint>
#include <stdbool>
#include <stddef.h>



#define ASHI_VIEW_VERSION "1.0.0"

//back-ground rendering modes for transparent pngs

typedef enum {
    BG_CHECKER = 0,
    BG_SOLID,
    BG_NONE
} ashi_bg_mode_t;


///background rgb color construct used for alpha blending and checkerboard rendering
typedef struct {
    ashi_bg_mode_t mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ashi_bg_color_t;



//single decoded img frame (RGBA 8 bit)

typedef struct {
    int width;
    int height;
    uint8_t *pixels; //point to raw32bit rgba bytes
    int delay_ms;  //frame duration
} ashi_frame_t;


//image container: one or more anim frame holder

typedef struct {
    int width;
    int height;
    int channels;
    int frame_count; //1 for static img
    ashi_frame_t *frames; //arr of frame
    bool is_animated;
    bool has_alpha;
    uint8_t *raw_data;
    size_t raw_size; //store orgnl comp file bytes
} ashi_image_t;


//cmd line exec config

typedef struct 
{
    const char *file_path;
    ashi_protocol_t protocol; //which terminal protocol
    int target_cols;
    int target_rows;
    ashi_bg_color_t bg; //bg color rgba struct
    int loop_count;  //will use them later
    float speed;
    bool clear_screen;
    bool show_info;
} ashi_config_t;





#endif
