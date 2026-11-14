#ifndef ASHI_RENDER_KITTY_H
#define ASHI_RENDER_KITTY_H

#include "ashi.h"

/* renders an image using the native Kitty Graphics Protocol.
 * emits chunked Base64 APC escape sequences (a=T, f=32 RGBA).
 * handles transparent PNGs natively or with background blending.
 * handles animated GIF playback with high-resolution frame pacing.
 * returns 0 on success. */
int ashi_render_kitty(const ashi_image_t *img, const ashi_config_t *cfg);

#endif //ASHI_RENDER_KITTY_H 
