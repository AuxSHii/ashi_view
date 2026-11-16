#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
#include "render_kitty.h"
#include "render_iterm.h"
#include "render_halfblock.h"

ashi_protocol_t ashi_detect_protocol(void){

//kitty detection
    if (getenv("KITTY_WINDOW_ID") || getenv("KITTY_PID")) {
        return PROTO_KITTY;
    }

    const char *term = getenv("TERM");
    if (term && (strstr(term, "kitty") || strstr(term, "ghostty"))) {
        return PROTO_KITTY;
    }

    // 2. iTerm2 / WezTerm / Mintty / Warp Detection 
    const char *term_program = getenv("TERM_PROGRAM");
    if (term_program) {
        if (strcmp(term_program, "iTerm.app") == 0 ||
            strcmp(term_program, "WezTerm") == 0 ||
            strcmp(term_program, "mintty") == 0 ||
            strcmp(term_program, "WarpTerminal") == 0) {
            return PROTO_ITERM;
        }
    }

    const char *lc_terminal = getenv("LC_TERMINAL");
    if (lc_terminal && strcmp(lc_terminal, "iTerm2") == 0) {
        return PROTO_ITERM;
    }

    // 3. Clean Unicode Half-Block TrueColor Fallback for standard terminals 
    return PROTO_HALFBLOCK;
}

const char *ashi_protocol_name(ashi_protocol_t proto)
{
    switch (proto) {
        case PROTO_KITTY:     return "Kitty Graphics Protocol";
        case PROTO_ITERM:     return "iTerm2 Inline Images Protocol";
        case PROTO_HALFBLOCK: return "Unicode Half-Block TrueColor Fallback";
        default:              return "Unknown";
    }
}

int ashi_render(const ashi_image_t *img, const ashi_config_t *cfg)
{
    if (!img || !cfg) return -1;

    ashi_protocol_t active_proto = cfg->protocol;
    if (active_proto == PROTO_AUTO) {
        active_proto = ashi_detect_protocol();
    }

    switch (active_proto) {
        case PROTO_KITTY:
            return ashi_render_kitty(img, cfg);
        case PROTO_ITERM:
            return ashi_render_iterm(img, cfg);
        case PROTO_HALFBLOCK:
        default:
            return ashi_render_halfblock(img, cfg);
    }
}
