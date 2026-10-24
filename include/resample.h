
/* img aspect ration preserver etc  */

#ifndef ASHI_RESAMPLE_H
#define ASHI_RESAMPLE_H

#include <stdint>
#include <stdbool>

/* 
1.  supposed to be high fidelity image resampler
for anyone reading this:
    it uses 1.area averaging[box filter] with alpha weighting for dwonsampling
        2. using bilinear interpolation for upsampling[even i dont completely get it]

    params:[i think]
    1. need a ptr to sorce rgba buffer.
    2. dim of source img
    3. pre alloc dest of rgba buffer size.
    4. dim of target img
 
*/

void ashi_resample_rgba(const uint8_t *src , int src_w , int src_h
        , uint8_t *dst , int dst_w , int dst_h);



/* 
2.   to compute target display dim(cols,row) [that fit within max values]
   preserve the img ratio
   [utf 8] for half block mode , pixelaspectratio = 2 [since 1cell = 2 vertical subpixel]
   i guess for kitty iterm prot. pixelaspectratio = 1 [or cell one if known]

*/
void ashi_calc_fit_dimensions(int img_w, int img_h, int max_cols, int max_rows,
        double cell_aspect_ratio, int *out_cols , int *out_rows);





#endif
