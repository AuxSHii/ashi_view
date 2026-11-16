#ifndef ASHI_PROTOCOL_H
#define ASHI_PROTOCOL_H

#include "ashi.h"

// auto detect terminal capabilities from env variables and terminal state
ashi_protocol_t ashi_detect_protocol(void);

//return a human readable protocol name

const char *ashi_detect_protocol(ashi_protocol_t proto);

//dispatch img rendering to the selected or auto detected protocol backend

int ashi_render(const ashi_image_t *img, const ashi_confif_t *cfg);

#endif 