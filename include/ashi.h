#ifndef ASHI_H
#define ASHI_H

#define ASHI_VIEW_VERSION "1.0.0"

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


#endif
