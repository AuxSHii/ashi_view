

#include "resample.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>


//fxn for calc fitting dim for img into usr terminal screen

void ashi_calc_fit_dimensions(int img_w, int img_h,
int max_cols, int max_rows,double cell_aspect_ratio, 
int *out_cols, int *out_rows){

    if(img_w <= 0 || img_h <= 0){
         *out_cols = 0;
         *out_rows = 0;
         return;
    } 



// end of fxn
}


