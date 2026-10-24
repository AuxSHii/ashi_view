

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



//fxn for resampling img from src to dst. 
//either down or up 
  
 void ashi_resample_rgba(const uint8_t *src, int src_w, int src_h,
         uint8_t *dst, int dst_w, int dst_h){
   
     //check if valid input or not
     if(!src || !dst || src_h <= 0 || src_w <= 0 || dst_h <= 0 || dst_w <= 0)
         return;

     //identity case
     if(src_w = dst_w && src_h == dst_h){
         memcpy(dst , src , (size_t)src_w * (size_t)src_h * 4);
         return;
     }
 //Downsampling img - area averaging box filter.
 
     if(dst_w <= src_w && dst_h <= src_h){
         for(int dy = 0 ; dy < dst_h; ++dy){  //for height
             //compute continious source intervals
             double y0 = (double)dy * (double)src_h / (double)dst_h;
             double y1 = (double)(dy+1) * (double)src_h / (double)dst_h;
            //aslo save these in int
            int iy0 = (int)y0;
            int iy1 = (int)ceil(y1);
            
            if(iy1 > src_h) iy1 = src_h; //clamped it to int boundary

            for(int dx = 0 ; dx < dst_w; ++dx){
                double x0 = (double)dx * (double)src_w / (double)dst_w;
                double x1 = (double)(dx+1) * (double)src_w / (double)dst_w;

                int ix0 = (int)x0;
                int ix1 = (int)ceil(x1);

                if(ix1 > src_w) ix1 = src_w;

                //inti accumulators

                double r_acc = 0.0 ;
                double g_acc = 0.0 , b_acc = 0.0 , a_acc = 0.0;

                double total_weight = 0.0;

                //for each int co-ordinate
                for(int sy = 0; sy < iy1 , sy++ ){
                    double y_weight = fmin(y1 , (double)(sy+1)) - fmax(y0, (double)sy);
                    if(y_weight <= 0.0) continue;


                    for (int sx = ix0; sx < ix1; ++sx) {
                        double x_weight = fmin(x1, (double)(sx + 1)) - fmax(x0, (double)sx);
                        if (x_weight <= 0.0) continue;

                        double w = x_weight * y_weight;
                        const uint8_t *sp = &src[(sy * src_w + sx) * 4];
                        double a = (double)sp[3];

                        // alpha weighted color accumulation avoids black halo fringing 
                        r_acc += (double)sp[0] * a * w;
                        g_acc += (double)sp[1] * a * w;
                        b_acc += (double)sp[2] * a * w;
                        a_acc += a * w;
                        total_weight += w;
                    }
                }

                uint8_t *dp = &dst[(dy * dst_w + dx) * 4];
                if (total_weight > 0.0) {
                    double final_a = a_acc / total_weight;
                    dp[3] = (uint8_t)(final_a + 0.5);

                    if (a_acc > 0.001) {
                        double r = r_acc / a_acc;
                        double g = g_acc / a_acc;
                        double b = b_acc / a_acc;
                        dp[0] = (uint8_t)(fmin(255.0, fmax(0.0, r)) + 0.5);
                        dp[1] = (uint8_t)(fmin(255.0, fmax(0.0, g)) + 0.5);
                        dp[2] = (uint8_t)(fmin(255.0, fmax(0.0, b)) + 0.5);
                    } else {
                        dp[0] = dp[1] = dp[2] = 0;
                    }
                } else {
                    dp[0] = dp[1] = dp[2] = dp[3] = 0;
                }
            }
        }
        return;
    }

    // bilinear interpolation fallback for upsampling 
    for (int dy = 0; dy < dst_h; ++dy) {
        double sy = ((double)dy + 0.5) * (double)src_h / (double)dst_h - 0.5;
        int y0 = (int)floor(sy);
        int y1 = y0 + 1;
        double wy = sy - (double)y0;
        if (y0 < 0) y0 = 0;
        if (y1 >= src_h) y1 = src_h - 1;

        for (int dx = 0; dx < dst_w; ++dx) {
            double sx = ((double)dx + 0.5) * (double)src_w / (double)dst_w - 0.5;
            int x0 = (int)floor(sx);
            int x1 = x0 + 1;
            double wx = sx - (double)x0;
            if (x0 < 0) x0 = 0;
            if (x1 >= src_w) x1 = src_w - 1;

            const uint8_t *p00 = &src[(y0 * src_w + x0) * 4];
            const uint8_t *p10 = &src[(y0 * src_w + x1) * 4];
            const uint8_t *p01 = &src[(y1 * src_w + x0) * 4];
            const uint8_t *p11 = &src[(y1 * src_w + x1) * 4];

            double w00 = (1.0 - wx) * (1.0 - wy);
            double w10 = wx * (1.0 - wy);
            double w01 = (1.0 - wx) * wy;
            double w11 = wx * wy;

            uint8_t *dp = &dst[(dy * dst_w + dx) * 4];
            for (int c = 0; c < 4; ++c) {
                double val = (double)p00[c] * w00 +
                             (double)p10[c] * w10 +
                             (double)p01[c] * w01 +
                             (double)p11[c] * w11;
                dp[c] = (uint8_t)(fmin(255.0, fmax(0.0, val)) + 0.5);
            }
        }
    }
 //end of fxn
}

        



    
 



