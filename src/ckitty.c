#define _POSIX_C_SOURCE 200809L

#include <ncurses.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ckitty_core.h"

#define CKITTY_VERSION "1.0.0"
#define DELAY_DEFAULT_US 40000
#define GROW_DELAY_DEFAULT_US 60000
#define MIN_DELAY_US 1000
#define SCREENSAVER_PERIOD_MS 8000ULL

typedef struct {
    int delay_us;
    int grow_delay_us;
    int live;
    int colors;
    int ascii;
    int rainbow;
    int screensaver;
    int infinite;
    int has_seed;
    uint32_t seed;
    const char* message;

    int dump;
    int dump_w;
    int dump_h;
    uint64_t dump_frame;
    int pose_override;  // -1 means random
} Config;

static uint64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }

    // Keep startup usable on older libc implementations without a monotonic
    // clock. This path is only a fallback; animation remains best-effort.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static void sleep_us(int delay_us) {
    struct timespec requested;
    requested.tv_sec = delay_us / 1000000;
    requested.tv_nsec = (long)(delay_us % 1000000) * 1000L;
    while (nanosleep(&requested, &requested) != 0 && errno == EINTR) {
        /* Resume after an interrupt. */
    }
}

static int parse_int(const char* s, int* out) {
    char* end = NULL;
    errno = 0;
    if (!s) return 0;
    long value = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || value < INT_MIN || value > INT_MAX) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int parse_u64(const char* s, uint64_t* out) {
    char* end = NULL;
    errno = 0;
    if (!s || s[0] == '-') return 0;
    unsigned long long value = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return 0;
    *out = (uint64_t)value;
    return 1;
}

static int parse_u32(const char* s, uint32_t* out) {
    uint64_t value = 0;
    if (!parse_u64(s, &value) || value > UINT32_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

static int parse_pose(const char* s) {
    if (!s) return -1;
    if (strcmp(s, "sit") == 0 || strcmp(s, "sitting") == 0) return CKPOSE_SIT;
    if (strcmp(s, "sleep") == 0 || strcmp(s, "sleeping") == 0) return CKPOSE_SLEEP;
    if (strcmp(s, "play") == 0 || strcmp(s, "playing") == 0) return CKPOSE_PLAY;
    if (strcmp(s, "walk") == 0 || strcmp(s, "walking") == 0) return CKPOSE_WALK;
    if (strcmp(s, "random") == 0) return -1;
    return -2;
}

static void print_usage(FILE* stream) {
    fprintf(stream, "ckitty %s - a procedural, animated terminal kitty\n\n", CKITTY_VERSION);
    fprintf(stream, "Usage: ckitty [POSE] [OPTIONS]\n\n");
    fprintf(stream, "Start here:\n");
    fprintf(stream, "  ckitty                 Bring a colorful kitty to life\n");
    fprintf(stream, "  ckitty play            Start with a playful pose\n");
    fprintf(stream, "  ckitty --ascii         Force readable, color-free ASCII\n\n");
    fprintf(stream, "Everyday options:\n");
    fprintf(stream, "  -h, --help             Show this help message\n");
    fprintf(stream, "  -a, --ascii            Disable color (also honors NO_COLOR)\n");
    fprintf(stream, "  -r, --rainbow          Add a gentle color shimmer\n");
    fprintf(stream, "  -l, --live             Reveal the kitty piece by piece\n");
    fprintf(stream, "  -S, --screensaver      Change kitties every few seconds\n");
    fprintf(stream, "  -p, --pose <name>      sit|sleep|play|walk|random\n");
    fprintf(stream, "  -m, --message <text>   Add a small message below the art\n\n");
    fprintf(stream, "Scripts and demos:\n");
    fprintf(stream, "  -s, --seed <num>       Set a reproducible unsigned 32-bit seed\n");
    fprintf(stream, "      --dump             Render one frame to stdout (no ncurses)\n");
    fprintf(stream, "      --frame <n>        Choose the frame for --dump (default: 0)\n");
    fprintf(stream, "      --version          Show the version\n\n");
    fprintf(stream, "Interactive controls:\n");
    fprintf(stream, "  q / ESC   quit     space   cycle pose     n   new kitty\n");
    fprintf(stream, "\nTip: use --dump with --seed, a pose, and --frame for scripts and CI.\n");
    fprintf(stream, "Advanced timing and canvas options remain available for demos.\n");
}

static int init_colors(void) {
    if (start_color() == ERR) return 0;
#ifdef NCURSES_VERSION
    use_default_colors();
#endif
    init_pair(CKCLR_FUR, COLOR_YELLOW, -1);
    init_pair(CKCLR_PAW, COLOR_WHITE, -1);
    init_pair(CKCLR_NOSE, COLOR_RED, -1);
    init_pair(CKCLR_TOY, COLOR_MAGENTA, -1);
    init_pair(CKCLR_ACCENT, COLOR_CYAN, -1);
    init_pair(CKCLR_GRAY, COLOR_WHITE, -1);
    init_pair(CKCLR_GROUND, COLOR_GREEN, -1);
    return 1;
}

static void draw_canvas_to_curses(const ckitty_canvas* canvas, const Config* cfg, uint64_t frame) {
    if (!canvas || !cfg) return;

    for (int y = 0; y < canvas->h; y++) {
        for (int x = 0; x < canvas->w; x++) {
            char ch = ckitty_canvas_get(canvas, x, y);
            if (ch == ' ') continue;

            uint8_t active = ckitty_canvas_get_color(canvas, x, y);
            int color_on = cfg->colors && !cfg->ascii;
            if (color_on && cfg->rainbow) {
                active = (uint8_t)(((frame / 10ULL) + (uint64_t)x + (uint64_t)y) % 7ULL + 1ULL);
            }
            if (color_on && active >= CKCLR_FUR && active <= CKCLR_GROUND) {
                attron(COLOR_PAIR(active));
            }

            (void)mvaddch(y, x, (chtype)(unsigned char)ch);

            if (color_on && active >= CKCLR_FUR && active <= CKCLR_GROUND) {
                attroff(COLOR_PAIR(active));
            }
        }
    }
}

static const char* pose_name(ckitty_pose pose) {
    switch (pose) {
        case CKPOSE_SIT:
            return "sit";
        case CKPOSE_SLEEP:
            return "sleep";
        case CKPOSE_PLAY:
            return "play";
        case CKPOSE_WALK:
        default:
            return "walk";
    }
}

static void draw_stage_frame(int width, int height, const Config* cfg) {
    if (!cfg || width < 12 || height < 8) return;

    int left = 2;
    int right = width - 3;
    int top = 2;
    int bottom = height - 3;
    int color_on = cfg->colors && !cfg->ascii;
    if (color_on) attron(COLOR_PAIR(CKCLR_GRAY) | A_DIM);

    (void)mvaddch(top, left, '+');
    (void)mvaddch(top, right, '+');
    (void)mvaddch(bottom, left, '+');
    (void)mvaddch(bottom, right, '+');
    for (int x = left + 1; x < right; x++) {
        (void)mvaddch(top, x, '-');
        (void)mvaddch(bottom, x, '-');
    }
    for (int y = top + 1; y < bottom; y++) {
        (void)mvaddch(y, left, '|');
        (void)mvaddch(y, right, '|');
    }

    if (color_on) attroff(COLOR_PAIR(CKCLR_GRAY) | A_DIM);
}

static void draw_topbar(int width, const Config* cfg, const ckitty_kitty* kitty, uint32_t seed) {
    if (!cfg || !kitty || width <= 0) return;
    int color_on = cfg->colors && !cfg->ascii;
    char meta[96];
    (void)snprintf(meta, sizeof(meta), " %s | seed %u ", pose_name(kitty->pose), (unsigned)seed);
    size_t meta_len = strlen(meta);

    if (color_on) attron(COLOR_PAIR(CKCLR_ACCENT) | A_BOLD);
    else attron(A_BOLD);
    (void)mvhline(0, 0, ' ', width);
    if (width > 4) (void)mvaddnstr(0, 2, " ckitty ", width - 4);
    if (width > 14 && meta_len < (size_t)(width - 14)) {
        int meta_x = width - (int)meta_len - 2;
        (void)mvaddnstr(0, meta_x, meta, (int)meta_len);
    }
    if (color_on) attroff(COLOR_PAIR(CKCLR_ACCENT) | A_BOLD);
    else attroff(A_BOLD);
}

static void draw_ground(int width, int height, const Config* cfg) {
    int y = height - 5;
    if (!cfg || width < 12 || height <= 0 || y < 0 || y >= height) return;
    int color_on = cfg->colors && !cfg->ascii;
    if (color_on) attron(COLOR_PAIR(CKCLR_GROUND) | A_DIM);
    for (int x = 4; x < width - 4; x++) {
        if ((x - 4) % 3 == 0) (void)mvaddch(y, x, '.');
    }
    if (color_on) attroff(COLOR_PAIR(CKCLR_GROUND) | A_DIM);
}

static void draw_centered_text(int y, int width, const char* text) {
    if (y < 0 || y >= LINES || width <= 0 || !text) return;
    size_t length = strlen(text);
    if (length > (size_t)INT_MAX) length = INT_MAX;
    int visible = (length > (size_t)width) ? width : (int)length;
    int x = (width - visible) / 2;
    (void)mvaddnstr(y, x, text, visible);
}

static void draw_footer(int width, int height, const Config* cfg, const ckitty_kitty* kitty, uint32_t seed) {
    if (!cfg || !kitty || height < 2 || width <= 0) return;
    int color_on = cfg->colors && !cfg->ascii;
    char left[96];
    const char* hints = " q quit   space pose   n new kitty ";
    (void)snprintf(left, sizeof(left), " %s | seed %u ", pose_name(kitty->pose), (unsigned)seed);

    if (color_on) attron(COLOR_PAIR(CKCLR_GRAY) | A_DIM);
    else attron(A_DIM);
    (void)mvhline(height - 2, 0, '-', width);
    (void)mvaddnstr(height - 1, 0, left, width);
    if (color_on) attroff(COLOR_PAIR(CKCLR_GRAY) | A_DIM);
    else attroff(A_DIM);

    if (color_on) attron(COLOR_PAIR(CKCLR_ACCENT));
    int hint_x = width - (int)strlen(hints);
    if (hint_x > (int)strlen(left) + 2) (void)mvaddnstr(height - 1, hint_x, hints, width - hint_x);
    if (color_on) attroff(COLOR_PAIR(CKCLR_ACCENT));
}

static void draw_tagline(int width, int height, const Config* cfg, const char* message, uint64_t frame) {
    if (!cfg || width <= 0 || height < 2) return;
    if (message) {
        draw_centered_text(1, width, message);
        return;
    }

    static const char* const dots[] = {"", ".", "..", "..."};
    char tagline[64];
    (void)snprintf(tagline, sizeof(tagline), "a tiny terminal cat%s", dots[(frame / 12ULL) % 4ULL]);
    int color_on = cfg->colors && !cfg->ascii;
    if (color_on) attron(COLOR_PAIR(CKCLR_GRAY));
    draw_centered_text(1, width, tagline);
    if (color_on) attroff(COLOR_PAIR(CKCLR_GRAY));
}

static void draw_ambient(int width, int height, const Config* cfg, uint32_t seed, uint64_t frame) {
    if (!cfg || width < 34 || height < 12) return;
    int color_on = cfg->colors && !cfg->ascii;
    int x_span = width - 12;
    int y_span = height - 10;
    uint32_t tick = (uint32_t)(frame / 10ULL);

    for (uint32_t i = 0; i < 5U; i++) {
        uint32_t value = seed ^ (0x9e3779b9U * (i + 1U)) ^ (tick * (0x45d9f3bU + i));
        value ^= value >> 16;
        int x = 6 + (int)(value % (uint32_t)x_span);
        int y = 4 + (int)((value >> 8) % (uint32_t)y_span);
        int phase = (int)((tick + i * 3U) % 7U);
        if (phase == 0 || phase == 1) {
            if (color_on) attron(COLOR_PAIR(CKCLR_ACCENT));
            (void)mvaddch(y, x, phase == 0 ? '*' : '+');
            if (color_on) attroff(COLOR_PAIR(CKCLR_ACCENT));
        } else {
            if (color_on) attron(COLOR_PAIR(CKCLR_GRAY) | A_DIM);
            else attron(A_DIM);
            (void)mvaddch(y, x, '.');
            if (color_on) attroff(COLOR_PAIR(CKCLR_GRAY) | A_DIM);
            else attroff(A_DIM);
        }
    }
}

static void draw_compact_notice(int width, int height, const Config* cfg) {
    if (!cfg || width <= 0 || height <= 0) return;
    int color_on = cfg->colors && !cfg->ascii;
    if (color_on) attron(COLOR_PAIR(CKCLR_ACCENT) | A_BOLD);
    else attron(A_BOLD);
    draw_centered_text(height / 2, width, "make room for kitty");
    if (color_on) attroff(COLOR_PAIR(CKCLR_ACCENT) | A_BOLD);
    else attroff(A_BOLD);
    if (height / 2 + 1 < height) draw_centered_text(height / 2 + 1, width, "resize to continue");
}

static void pick_anchor(int width, int height, int randomize, ckitty_rng* rng, int* out_cx, int* out_cy) {
    int cx = width / 2;
    int cy = height / 2;
    if (randomize && rng) {
        int cx_min = 12;
        int cx_max = width - 13;
        int cy_min = 6;
        int cy_max = height - 9;
        if (cx_max < cx_min) cx_min = cx_max = width / 2;
        if (cy_max < cy_min) cy_min = cy_max = height / 2;
        cx = cx_min + ckitty_rng_range(rng, cx_max - cx_min + 1);
        cy = cy_min + ckitty_rng_range(rng, cy_max - cy_min + 1);
    }
    *out_cx = cx;
    *out_cy = cy;
}

static int prepare_live(const ckitty_kitty* kitty, ckitty_canvas* full, ckitty_draw_order* order) {
    if (!kitty || !full || !order) return 0;
    ckitty_render_frame(kitty, 0, full);
    return ckitty_draw_order_build(full, kitty->seed, order);
}

static int resize_canvas(ckitty_canvas* canvas, int width, int height) {
    ckitty_canvas replacement = {0};
    if (!canvas || !ckitty_canvas_init(&replacement, width, height)) return 0;
    ckitty_canvas_free(canvas);
    *canvas = replacement;
    return 1;
}

static int resize_live_canvases(ckitty_canvas* full, ckitty_canvas* visible, int width, int height) {
    ckitty_canvas new_full = {0};
    ckitty_canvas new_visible = {0};
    if (!full || !visible || !ckitty_canvas_init(&new_full, width, height) ||
        !ckitty_canvas_init(&new_visible, width, height)) {
        ckitty_canvas_free(&new_full);
        ckitty_canvas_free(&new_visible);
        return 0;
    }
    ckitty_canvas_free(full);
    ckitty_canvas_free(visible);
    *full = new_full;
    *visible = new_visible;
    return 1;
}

static int rebuild_live(const ckitty_kitty* kitty, ckitty_canvas* full, ckitty_draw_order* order,
                        int* visible, int* grown) {
    if (!kitty || !full || !order || !visible || !grown) return 0;
    ckitty_draw_order_free(order);
    if (!prepare_live(kitty, full, order)) return 0;
    *visible = 0;
    *grown = 0;
    return 1;
}

static void choose_kitty(ckitty_kitty* kitty, ckitty_rng* rng, int width, int height,
                         int randomize, int pose_override) {
    int cx = 0;
    int cy = 0;
    pick_anchor(width, height, randomize, rng, &cx, &cy);
    ckitty_kitty_randomize(kitty, rng, cx, cy);
    if (pose_override >= 0) kitty->pose = (ckitty_pose)pose_override;
}

int main(int argc, char* argv[]) {
    Config cfg = {
        .delay_us = DELAY_DEFAULT_US,
        .grow_delay_us = GROW_DELAY_DEFAULT_US,
        .live = 0,
        .colors = 1,
        .ascii = getenv("NO_COLOR") != NULL,
        .rainbow = 0,
        .screensaver = 0,
        .infinite = 1,
        .has_seed = 0,
        .seed = 0,
        .message = NULL,
        .dump = 0,
        .dump_w = 80,
        .dump_h = 24,
        .dump_frame = 0,
        .pose_override = -1
    };

    enum {
        OPT_DUMP = 1000,
        OPT_WIDTH,
        OPT_HEIGHT,
        OPT_FRAME,
        OPT_VERSION
    };
    static const struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"colors", no_argument, 0, 'c'},
        {"ascii", no_argument, 0, 'a'},
        {"rainbow", no_argument, 0, 'r'},
        {"live", no_argument, 0, 'l'},
        {"screensaver", no_argument, 0, 'S'},
        {"infinite", no_argument, 0, 'i'},
        {"seed", required_argument, 0, 's'},
        {"delay", required_argument, 0, 'd'},
        {"grow-delay", required_argument, 0, 'g'},
        {"pose", required_argument, 0, 'p'},
        {"message", required_argument, 0, 'm'},
        {"dump", no_argument, 0, OPT_DUMP},
        {"width", required_argument, 0, OPT_WIDTH},
        {"height", required_argument, 0, OPT_HEIGHT},
        {"frame", required_argument, 0, OPT_FRAME},
        {"version", no_argument, 0, OPT_VERSION},
        {0, 0, 0, 0}
    };

    opterr = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "hcarlSi:s:d:g:p:m:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(stdout);
                return 0;
            case 'c':
                cfg.colors = 1;
                break;
            case 'a':
                cfg.ascii = 1;
                break;
            case 'r':
                cfg.rainbow = 1;
                cfg.colors = 1;
                break;
            case 'l':
                cfg.live = 1;
                break;
            case 'S':
                cfg.screensaver = 1;
                cfg.infinite = 1;
                break;
            case 'i':
                cfg.infinite = 1;
                break;
            case 's':
                if (!parse_u32(optarg, &cfg.seed)) {
                    fprintf(stderr, "ckitty: invalid seed: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                cfg.has_seed = 1;
                break;
            case 'd':
                if (!parse_int(optarg, &cfg.delay_us) || cfg.delay_us < 0) {
                    fprintf(stderr, "ckitty: invalid delay: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                break;
            case 'g':
                if (!parse_int(optarg, &cfg.grow_delay_us) || cfg.grow_delay_us < 0) {
                    fprintf(stderr, "ckitty: invalid grow delay: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                break;
            case 'p': {
                int pose = parse_pose(optarg);
                if (pose == -2) {
                    fprintf(stderr, "ckitty: invalid pose: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                cfg.pose_override = pose;
                break;
            }
            case 'm':
                cfg.message = optarg;
                break;
            case OPT_DUMP:
                cfg.dump = 1;
                break;
            case OPT_WIDTH:
                if (!parse_int(optarg, &cfg.dump_w) || cfg.dump_w <= 0) {
                    fprintf(stderr, "ckitty: invalid width: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                break;
            case OPT_HEIGHT:
                if (!parse_int(optarg, &cfg.dump_h) || cfg.dump_h <= 0) {
                    fprintf(stderr, "ckitty: invalid height: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                break;
            case OPT_FRAME:
                if (!parse_u64(optarg, &cfg.dump_frame)) {
                    fprintf(stderr, "ckitty: invalid frame: %s\n", optarg ? optarg : "(missing)");
                    return 2;
                }
                break;
            case OPT_VERSION:
                printf("ckitty %s\n", CKITTY_VERSION);
                return 0;
            case '?':
            default:
                fprintf(stderr, "ckitty: unknown or incomplete option\n");
                print_usage(stderr);
                return 2;
        }
    }

    if (optind < argc) {
        if (optind + 1 != argc || cfg.pose_override >= 0) {
            fprintf(stderr, "ckitty: expected one pose (sit, sleep, play, or walk)\n");
            return 2;
        }
        int pose = parse_pose(argv[optind]);
        if (pose == -2) {
            fprintf(stderr, "ckitty: invalid pose: %s\n", argv[optind]);
            return 2;
        }
        cfg.pose_override = pose;
    }
    if (cfg.delay_us < MIN_DELAY_US) cfg.delay_us = MIN_DELAY_US;
    if (cfg.grow_delay_us < MIN_DELAY_US) cfg.grow_delay_us = MIN_DELAY_US;

    uint32_t seed = cfg.has_seed ? cfg.seed : (uint32_t)time(NULL) ^ (uint32_t)getpid();

    if (cfg.dump) {
        ckitty_canvas canvas = {0};
        if (!ckitty_canvas_init(&canvas, cfg.dump_w, cfg.dump_h)) {
            fprintf(stderr, "ckitty: canvas is too large or could not be allocated (%dx%d)\n",
                    cfg.dump_w, cfg.dump_h);
            return 1;
        }

        ckitty_rng rng;
        ckitty_rng_seed(&rng, seed);
        ckitty_kitty kitty;
        ckitty_kitty_randomize(&kitty, &rng, cfg.dump_w / 2, cfg.dump_h / 2);
        if (cfg.pose_override >= 0) kitty.pose = (ckitty_pose)cfg.pose_override;
        ckitty_render_frame(&kitty, cfg.dump_frame, &canvas);

        char* output = ckitty_canvas_dump_bbox(&canvas);
        if (!output) {
            fprintf(stderr, "ckitty: failed to allocate dump output\n");
            ckitty_canvas_free(&canvas);
            return 1;
        }
        fputs(output, stdout);
        free(output);
        ckitty_canvas_free(&canvas);
        return 0;
    }

    if (initscr() == NULL) {
        fprintf(stderr, "ckitty: could not initialize the terminal\n");
        return 1;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    int width = 0;
    int height = 0;
    getmaxyx(stdscr, height, width);
    if (width <= 0 || height <= 0) {
        endwin();
        fprintf(stderr, "ckitty: terminal has no usable rows or columns\n");
        return 1;
    }

    if (cfg.colors && !cfg.ascii && has_colors()) {
        if (!init_colors()) {
            cfg.colors = 0;
            cfg.rainbow = 0;
        }
    } else {
        cfg.colors = 0;
        cfg.rainbow = 0;
    }

    ckitty_canvas canvas = {0};
    ckitty_canvas live_full = {0};
    ckitty_canvas live_visible_canvas = {0};
    if (!ckitty_canvas_init(&canvas, width, height) ||
        (cfg.live && !resize_live_canvases(&live_full, &live_visible_canvas, width, height))) {
        ckitty_canvas_free(&canvas);
        ckitty_canvas_free(&live_full);
        ckitty_canvas_free(&live_visible_canvas);
        endwin();
        fprintf(stderr, "ckitty: could not allocate terminal canvas (%dx%d)\n", width, height);
        return 1;
    }

    ckitty_rng rng;
    ckitty_rng_seed(&rng, seed);
    ckitty_kitty kitty;
    choose_kitty(&kitty, &rng, width, height, cfg.screensaver, cfg.pose_override);

    ckitty_draw_order order = {0};
    int live_visible = 0;
    int grown = cfg.live ? 0 : 1;
    if (cfg.live && !rebuild_live(&kitty, &live_full, &order, &live_visible, &grown)) {
        ckitty_canvas_free(&canvas);
        ckitty_canvas_free(&live_full);
        ckitty_canvas_free(&live_visible_canvas);
        endwin();
        fprintf(stderr, "ckitty: could not prepare live rendering\n");
        return 1;
    }

    uint64_t next_spawn = now_ms() + SCREENSAVER_PERIOD_MS;
    uint64_t frame = 0;
    int exit_code = 0;
    int running = 1;

    while (running) {
        int new_width = 0;
        int new_height = 0;
        getmaxyx(stdscr, new_height, new_width);
        if (new_width <= 0 || new_height <= 0) {
            sleep_us(cfg.delay_us);
            continue;
        }
        if (new_width != width || new_height != height) {
            if (!resize_canvas(&canvas, new_width, new_height) ||
                (cfg.live && !resize_live_canvases(&live_full, &live_visible_canvas,
                                                     new_width, new_height))) {
                exit_code = 1;
                break;
            }
            width = new_width;
            height = new_height;
            pick_anchor(width, height, cfg.screensaver, &rng, &kitty.cx, &kitty.cy);
            if (cfg.live && !rebuild_live(&kitty, &live_full, &order, &live_visible, &grown)) {
                exit_code = 1;
                break;
            }
        }

        if (cfg.screensaver && now_ms() >= next_spawn) {
            choose_kitty(&kitty, &rng, width, height, 1, cfg.pose_override);
            frame = 0;
            next_spawn = now_ms() + SCREENSAVER_PERIOD_MS;
            if (cfg.live && !rebuild_live(&kitty, &live_full, &order, &live_visible, &grown)) {
                exit_code = 1;
                break;
            }
        }

        erase();
        draw_topbar(width, &cfg, &kitty, kitty.seed);
        draw_tagline(width, height, &cfg, cfg.message, frame);

        int compact = width < 34 || height < 12;
        if (compact) {
            draw_compact_notice(width, height, &cfg);
        } else {
            draw_stage_frame(width, height, &cfg);
            draw_ground(width, height, &cfg);
            draw_ambient(width, height, &cfg, kitty.seed, frame);

            const ckitty_canvas* to_draw = &canvas;
            if (cfg.live && !grown) {
                int step = order.len / 80 + 1;
                live_visible += step;
                if (live_visible >= order.len) {
                    live_visible = order.len;
                    grown = 1;
                }
                ckitty_canvas_copy_visible(&live_full, &order, live_visible, &live_visible_canvas);
                to_draw = &live_visible_canvas;
            } else {
                if (kitty.pose == CKPOSE_WALK && frame % 3ULL == 0) {
                    int min_cx = 12;
                    int max_cx = width - 13;
                    if (max_cx < min_cx) min_cx = max_cx = width / 2;
                    kitty.cx += kitty.facing;
                    if (kitty.cx <= min_cx) {
                        kitty.cx = min_cx;
                        kitty.facing = 1;
                    } else if (kitty.cx >= max_cx) {
                        kitty.cx = max_cx;
                        kitty.facing = -1;
                    }
                }
                ckitty_render_frame(&kitty, frame, &canvas);
                to_draw = &canvas;
            }

            draw_canvas_to_curses(to_draw, &cfg, frame);
        }
        draw_footer(width, height, &cfg, &kitty, kitty.seed);
        refresh();

        int input = getch();
        if (input == 'q' || input == 27) {
            running = 0;
        } else if (input == ' ') {
            kitty.pose = (ckitty_pose)(((int)kitty.pose + 1) % 4);
            frame = 0;
            if (cfg.live && !rebuild_live(&kitty, &live_full, &order, &live_visible, &grown)) {
                exit_code = 1;
                break;
            }
        } else if (input == 'n') {
            choose_kitty(&kitty, &rng, width, height, cfg.screensaver, cfg.pose_override);
            frame = 0;
            next_spawn = now_ms() + SCREENSAVER_PERIOD_MS;
            if (cfg.live && !rebuild_live(&kitty, &live_full, &order, &live_visible, &grown)) {
                exit_code = 1;
                break;
            }
        }

        if (!cfg.infinite && !cfg.screensaver && frame > 600ULL) running = 0;
        sleep_us((cfg.live && !grown) ? cfg.grow_delay_us : cfg.delay_us);
        frame++;
    }

    ckitty_draw_order_free(&order);
    ckitty_canvas_free(&canvas);
    ckitty_canvas_free(&live_full);
    ckitty_canvas_free(&live_visible_canvas);
    endwin();
    return exit_code;
}
