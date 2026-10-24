

#include "resample.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>


//fxn for calc fitting dim for img into usr terminal screen

void ashi_calc_fit_dimensions(int img_w, int img_h,
int max_cols, int max_rows,double cell_aspect_ratio, 
int *out_cols, int *out_rows){
  //boundary saftey
    if(img_w <= 0 || img_h <= 0 || max_cols <= 0 || max_rows <= 0){
         *out_cols = 1;
         *out_rows = 1;
         return;
    } 

 //effective display bounds in subpixel spcace i.e the divison
 double effective_h = (double)max_rows * cell_aspect_ratio;
 double scale_w = (double)max_cols / (double)img_w;
 double scale_h = effective_h / (double)img_h;

 double scale = (scale_w < scale_h) ? scale_w : scale_h;

//multiplied h and w with found scale to scale them accordingly
//round to nearest int
 int cols = ((double)img_w * scale + 0.5);
 int subpixel_h = (int)((double)img_h * scale + 0.5);
 int rows = (int)(((double)subpixel_h / cell_aspect_ratio) + 0.5);
 
 //clamp them btw 1 to max values
 if(cols < 1) cols = 1;
 if(rows < 1) rows = 1;

 if(cols > max_cols) cols = max_cols;
 if(rows > max_rows) rows = max_rows;

// store final vals

 *out_cols = cols;
 *out_rows = rows;
// end of fxn
}

//fxn for resampling img


void ashi_resample_rgba(const uint8_t *src, int src_w, int src_h,
        uint8_t *dst, int dst_w, int dst_h){



    //end of fxn
}






