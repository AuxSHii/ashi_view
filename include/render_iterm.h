#ifndef ASHI_RENDER_ITERM_H
#define ASHI_RENDER_ITERM_H

#include "ashi.h"

/* 
render an img using iterm2 inline image protocol [OSC 1337]
support direct pass through of raw PNG/GIF/JPEG streams or in-memory png generation
suppoerted by iterm2, wezterm, Mintty, warp, etc
return 0 on success
*/

int ashi_render_iterm(const ashi_image_t *img , const ashi_config_t *cfg);

#endif