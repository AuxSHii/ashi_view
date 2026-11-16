#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ashi.h"
#include "image.h"
#include "protocol.h"
#include "terminal.h"

static void print_usage(const char *prog)
{
    printf("ashi_view %s - A blazing-fast CLI image viewer\n\n", ASHI_VIEW_VERSION);
    printf("Usage:\n");
    printf("  %s [options] <image-file>\n\n", prog);
    printf("Protocols:\n");
    printf("  -p, --protocol <name>   Protocol backend: auto, kitty, iterm, halfblock (default: auto)\n\n");
    printf("Dimensions:\n");
    printf("  -w, --width <cols>      Target width in terminal columns (default: auto fit)\n");
    printf("  -h, --height <rows>     Target height in terminal rows (default: auto fit)\n\n");
    printf("Transparency & Background:\n");
    printf("  -b, --bg <mode>         Background for transparent PNGs: checker (default), black, white, none, or #RRGGBB\n\n");
    printf("Animations (GIF):\n");
    printf("  -l, --loop              Loop animation indefinitely (default for GIFs, press 'q' or Ctrl+C to exit)\n");
    printf("  -1, --once              Play animation exactly once\n");
    printf("  -n, --loops <count>     Play animation exactly <count> times\n");
    printf("  -s, --speed <float>     Animation playback speed multiplier (default: 1.0)\n\n");
    printf("Miscellaneous:\n");
    printf("  -c, --clear             Clear terminal screen before rendering\n");
    printf("  -i, --info              Print image metadata without rendering\n");
    printf("  -v, --version           Print version information\n");
    printf("      --help              Display this help menu\n\n");
    printf("Examples:\n");
    printf("  %s photo.png\n", prog);
    printf("  %s -b checker logo_transparent.png\n", prog);
    printf("  %s -p halfblock -w 60 cat.gif\n", prog);
    printf("  %s -s 1.5 -n 3 animation.gif\n", prog);
}

static int parse_bg(const char *arg, ashi_bg_color_t *bg)
{
    if (strcmp(arg, "checker") == 0) {
        bg->mode = BG_CHECKER;
        return 0;
    }
    if (strcmp(arg, "none") == 0) {
        bg->mode = BG_NONE;
        return 0;
    }
    if (strcmp(arg, "black") == 0) {
        bg->mode = BG_SOLID;
        bg->r = bg->g = bg->b = 0;
        return 0;
    }
    if (strcmp(arg, "white") == 0) {
        bg->mode = BG_SOLID;
        bg->r = bg->g = bg->b = 255;
        return 0;
    }
    if (arg[0] == '#') {
        unsigned int r = 0, g = 0, b = 0;
        if (sscanf(arg, "#%02x%02x%02x", &r, &g, &b) == 3) {
            bg->mode = BG_SOLID;
            bg->r = (uint8_t)r;
            bg->g = (uint8_t)g;
            bg->b = (uint8_t)b;
            return 0;
        }
    }
    fprintf(stderr, "ashi_view: unrecognized background option '%s'\n", arg);
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    ashi_config_t cfg = {
        .file_path = NULL,
        .protocol = PROTO_AUTO,
        .target_cols = 0,
        .target_rows = 0,
        .bg = { .mode = BG_CHECKER, .r = 0, .g = 0, .b = 0 },
        .loop_count = 0,
        .speed = 1.0f,
        .clear_screen = false,
        .show_info = false
    };

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            printf("ashi_view version %s\n", ASHI_VIEW_VERSION);
            return 0;
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--clear") == 0) {
            cfg.clear_screen = true;
        } else if (strcmp(arg, "-i") == 0 || strcmp(arg, "--info") == 0) {
            cfg.show_info = true;
        } else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--loop") == 0) {
            cfg.loop_count = -1;
        } else if (strcmp(arg, "-1") == 0 || strcmp(arg, "--once") == 0) {
            cfg.loop_count = 1;
        } else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--protocol") == 0) {
            if (++i >= argc) { fprintf(stderr, "ashi_view: missing protocol argument\n"); return 1; }
            if (strcmp(argv[i], "auto") == 0) cfg.protocol = PROTO_AUTO;
            else if (strcmp(argv[i], "kitty") == 0) cfg.protocol = PROTO_KITTY;
            else if (strcmp(argv[i], "iterm") == 0) cfg.protocol = PROTO_ITERM;
            else if (strcmp(argv[i], "halfblock") == 0) cfg.protocol = PROTO_HALFBLOCK;
            else { fprintf(stderr, "ashi_view: unknown protocol '%s'\n", argv[i]); return 1; }
        } else if (strcmp(arg, "-w") == 0 || strcmp(arg, "--width") == 0) {
            if (++i >= argc) { fprintf(stderr, "ashi_view: missing width argument\n"); return 1; }
            cfg.target_cols = atoi(argv[i]);
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--height") == 0) {
            if (++i >= argc) { fprintf(stderr, "ashi_view: missing height argument\n"); return 1; }
            cfg.target_rows = atoi(argv[i]);
        } else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--bg") == 0) {
            if (++i >= argc) { fprintf(stderr, "ashi_view: missing background argument\n"); return 1; }
            if (parse_bg(argv[i], &cfg.bg) != 0) return 1;
        } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--loops") == 0) {
            if (++i >= argc) { fprintf(stderr, "ashi_view: missing loops count\n"); return 1; }
            cfg.loop_count = atoi(argv[i]);
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--speed") == 0) {
            if (++i >= argc) { fprintf(stderr, "ashi_view: missing speed argument\n"); return 1; }
            cfg.speed = (float)atof(argv[i]);
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "ashi_view: unrecognized flag '%s'\n", arg);
            return 1;
        } else {
            cfg.file_path = arg;
        }
    }

    if (!cfg.file_path) {
        fprintf(stderr, "ashi_view: no input image file provided\n");
        print_usage(argv[0]);
        return 1;
    }

    terminal_init();

    ashi_image_t img;
    if (ashi_image_load(cfg.file_path, &img) != 0) {
        terminal_restore();
        return 1;
    }

    if (cfg.show_info) {
        ashi_protocol_t active_proto = (cfg.protocol == PROTO_AUTO) ? ashi_detect_protocol() : cfg.protocol;
        printf("File:        %s\n", cfg.file_path);
        printf("Dimensions:  %d x %d px\n", img.width, img.height);
        printf("Channels:    RGBA (8-bit)\n");
        printf("Has Alpha:   %s\n", img.has_alpha ? "yes" : "no");
        printf("Animated:    %s\n", img.is_animated ? "yes" : "no");
        if (img.is_animated) {
            printf("Frames:      %d\n", img.frame_count);
        }
        printf("File Size:   %zu bytes\n", img.raw_size);
        printf("Protocol:    %s\n", ashi_protocol_name(active_proto));
        ashi_image_free(&img);
        terminal_restore();
        return 0;
    }

    int ret = ashi_render(&img, &cfg);

    ashi_image_free(&img);
    terminal_restore();

    return ret;
}
