#define _XOPEN_SOURCE 700

#include <ncurses.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

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
    int has_seed;
    uint32_t seed;
    const char* message;

    int dump;
    int dump_w;
    int dump_h;
    uint64_t dump_frame;
    int theme;
    int quiet;
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
    fprintf(stream, "      --theme <name>     amber|moon|forest (default: amber)\n");
    fprintf(stream, "      --quiet            Start with the interface hidden\n");
    fprintf(stream, "  -r, --rainbow          Add a gentle color shimmer\n");
    fprintf(stream, "  -l, --live             Reveal the kitty piece by piece\n");
    fprintf(stream, "  -S, --screensaver      Change kitties every few seconds\n");
    fprintf(stream, "  -p, --pose <name>      sit|sleep|play|walk|random\n");
    fprintf(stream, "  -m, --message <text>   Add a small message under the title\n\n");
    fprintf(stream, "Scripts and demos:\n");
    fprintf(stream, "  -s, --seed <num>       Set a reproducible unsigned 32-bit seed\n");
    fprintf(stream, "      --dump             Render one frame to stdout (no ncurses)\n");
    fprintf(stream, "      --frame <n>        Choose the frame for --dump (default: 0)\n");
    fprintf(stream, "      --version          Show the version\n\n");
    fprintf(stream, "Interactive controls:\n");
    fprintf(stream, "  space / 1-4   pose     n   new kitty     p   pause / resume\n");
    fprintf(stream, "  t   palette     h   hide interface     ?   help\n");
    fprintf(stream, "  q   quit     ESC   close help, or quit\n");
    fprintf(stream, "\nTip: use --dump with --seed, a pose, and --frame for scripts and CI.\n");
    fprintf(stream, "Advanced timing and canvas options remain available for demos.\n");
}

/* Palettes use terminal-owned backgrounds, including transparent terminals.
 * Never redefine the user's base colors. */
typedef struct {
    const char* name;
    short rich[7];
    short basic[7];
} Theme;

static const Theme themes[] = {
    {"amber", {222, 230, 210, 181, 114, 245, 240},
     {COLOR_YELLOW, COLOR_WHITE, COLOR_RED, COLOR_MAGENTA, COLOR_GREEN, COLOR_WHITE, COLOR_WHITE}},
    {"moon", {153, 195, 218, 183, 147, 245, 240},
     {COLOR_CYAN, COLOR_WHITE, COLOR_RED, COLOR_MAGENTA, COLOR_BLUE, COLOR_WHITE, COLOR_WHITE}},
    {"forest", {151, 230, 216, 180, 109, 245, 240},
     {COLOR_GREEN, COLOR_WHITE, COLOR_RED, COLOR_YELLOW, COLOR_CYAN, COLOR_WHITE, COLOR_WHITE}}
};
#define THEME_COUNT ((int)(sizeof(themes) / sizeof(themes[0])))

static int parse_theme(const char* name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(name, themes[i].name) == 0) return i;
    }
    return -1;
}

static int apply_theme(int theme) {
    if (COLORS < 8 || COLOR_PAIRS <= CKCLR_GROUND) return 0;
    short background = COLOR_BLACK;
#ifdef NCURSES_VERSION
    if (use_default_colors() != ERR) background = -1;
#endif
    for (short pair = CKCLR_FUR; pair <= CKCLR_GROUND; pair++) {
        short foreground = COLORS >= 256 ? themes[theme].rich[pair - 1] : themes[theme].basic[pair - 1];
        if (init_pair(pair, foreground, background) == ERR) return 0;
    }
    return 1;
}

static void style(const Config* cfg, int color, int bold) {
    attrset(A_NORMAL);
    if (cfg->colors && !cfg->ascii) attron(COLOR_PAIR(color));
    if (bold) attron(A_BOLD);
}

/* Messages are data: control bytes must never move the cursor or wrap into
 * another UI row. Art and interface labels deliberately stay plain ASCII. */
static void text_at(int y, int x, int limit, const char* text) {
    if (!text || limit <= 0 || y < 0 || y >= LINES || x < 0 || x >= COLS) return;
    if (limit > COLS - x) limit = COLS - x;
    (void)move(y, x);
    mbstate_t state = {0};
    size_t remaining = strlen(text);
    int columns = 0;
    while (remaining > 0) {
        wchar_t ch;
        size_t bytes = mbrtowc(&ch, text, remaining, &state);
        if (bytes == (size_t)-1 || bytes == (size_t)-2) {
            /* Invalid input or a non-UTF-8 locale still gets safe, bounded text. */
            state = (mbstate_t){0};
            bytes = 1;
            ch = L'?';
        }
        if (bytes == 0) break;
        int cells = wcwidth(ch);
        if (cells < 0) { ch = L' '; cells = 1; }
        if (columns + cells > limit) break;
        /* A leading combining mark must not attach to an unrelated UI cell. */
        if (cells > 0 || columns > 0) (void)addnwstr(&ch, 1);
        columns += cells;
        text += bytes;
        remaining -= bytes;
    }
}

static void centered(int y, int width, const char* text) {
    int len = (int)strnlen(text, (size_t)width);
    text_at(y, (width - len) / 2, len, text);
}

static const char* pose_name(ckitty_pose pose) {
    static const char* const names[] = {"sit", "sleep", "play", "walk"};
    return names[(int)pose];
}

static const char* pose_mood(ckitty_pose pose) {
    static const char* const moods[] = {
        "watching the world", "do not disturb", "one more pounce", "a little wander"
    };
    return moods[(int)pose];
}

typedef struct {
    int left, right, top, bottom;
    int small;
} Stage;

static Stage stage_layout(int width, int height, const Config* cfg) {
    Stage stage = {2, width - 3, cfg->quiet ? 1 : 5,
                   height - (cfg->quiet ? 2 : 5), 0};
    stage.small = stage.right - stage.left < 39 || stage.bottom - stage.top < 9;
    return stage;
}

static void draw_chrome(int width, int height, const Config* cfg,
                        const ckitty_kitty* kitty, uint32_t seed, int paused, int growing) {
    if (cfg->quiet || width < 20 || height < 8) return;
    int narrow = width < 54 || height < 16;
    style(cfg, CKCLR_FUR, 1);
    text_at(0, 2, width - 4, "ckitty");
    style(cfg, CKCLR_ACCENT, 0);
    text_at(0, width - (int)strlen(themes[cfg->theme].name) - 2, 8, themes[cfg->theme].name);
    if (!narrow) {
        style(cfg, CKCLR_GRAY, 0);
        text_at(1, 2, width - 4, cfg->message ? cfg->message : "a little company, right here.");
        int x = 2;
        for (int i = 0; i < 4; i++) {
            char tab[24];
            (void)snprintf(tab, sizeof(tab), i == (int)kitty->pose ? "[%d %s]" : " %d %s ",
                           i + 1, pose_name((ckitty_pose)i));
            style(cfg, i == (int)kitty->pose ? CKCLR_FUR : CKCLR_GRAY, i == (int)kitty->pose);
            text_at(3, x, width - x - 2, tab);
            x += (int)strlen(tab) + 2;
        }
        style(cfg, CKCLR_GROUND, 0);
        (void)mvhline(height - 3, 2, '-', width - 4);
    } else if (cfg->message) {
        style(cfg, CKCLR_GRAY, 0);
        text_at(1, 2, width - 4, cfg->message);
    }
    char status[128];
    (void)snprintf(status, sizeof(status), "%s / %s", pose_name(kitty->pose),
                   paused ? "paused" : (growing ? "growing, then settling in" : pose_mood(kitty->pose)));
    style(cfg, paused ? CKCLR_ACCENT : CKCLR_GRAY, paused);
    text_at(height - 2, 2, width - 4, status);
    if (!narrow) {
        char meta[48];
        (void)snprintf(meta, sizeof(meta), "seed %u", (unsigned)seed);
        int meta_x = width - (int)strlen(meta) - 2;
        if (meta_x > 2 + (int)strlen(status) + 2) {
            style(cfg, CKCLR_GRAY, 0);
            text_at(height - 2, meta_x, (int)strlen(meta), meta);
        }
    }
    style(cfg, CKCLR_ACCENT, 0);
    const char* hints = width >= 78 ? "space pose   n new   p pause   t palette   h hide   ? help   q quit" :
                        width >= 42 ? "space pose  p pause  ? help  q quit" : "? help  q quit";
    text_at(height - 1, 2, width - 4, hints);
}

static void draw_ambient(const Stage* stage, const Config* cfg, uint32_t seed, uint64_t frame) {
    int span = stage->right - stage->left - 4;
    int sky = (stage->bottom - stage->top) / 2;
    if (stage->small || span <= 0 || sky <= 0) return;
    for (uint32_t i = 0; i < 7U; i++) {
        uint32_t value = seed ^ (0x9e3779b9U * (i + 1U));
        value ^= value >> 16;
        int x = stage->left + 2 + (int)(value % (uint32_t)span);
        int y = stage->top + (int)((value >> 8) % (uint32_t)sky);
        int bright = (frame / 24ULL + i * 5U) % 13ULL == 0;
        style(cfg, bright ? CKCLR_ACCENT : CKCLR_GROUND, 0);
        (void)mvaddch(y, x, bright ? '+' : '.');
    }
}

static void draw_small_kitty(int width, int height, const Config* cfg,
                            ckitty_pose pose, uint64_t frame) {
    style(cfg, CKCLR_FUR, 1);
    if (width < 20 || height < 8) {
        centered(height / 2, width, "make room for kitty");
        style(cfg, CKCLR_GRAY, 0);
        if (height / 2 + 1 < height) centered(height / 2 + 1, width, "resize to continue");
        return;
    }
    int y = height / 2 - 1;
    int x = (width - 7) / 2;
    centered(y, width, " /\\_/\\ ");
    style(cfg, CKCLR_PAW, 0);
    text_at(y + 1, x, 7, pose == CKPOSE_SLEEP || frame % 100ULL < 3ULL ?
            "( -.- )" : "( o.o )");
    if (pose == CKPOSE_SLEEP) text_at(y + 1, x + 8, 1, "z");
    style(cfg, CKCLR_FUR, 0);
    centered(y + 2, width, pose == CKPOSE_PLAY ? " / > @ " : " (___)~");
}

static void draw_help(int width, int height, const Config* cfg) {
    if (width < 20 || height < 8) {
        erase();
        style(cfg, CKCLR_FUR, 1);
        centered(height / 2 - 1, width, "make yourself at home");
        style(cfg, CKCLR_GRAY, 0);
        centered(height / 2, width, "resize for all controls");
        centered(height / 2 + 1, width, "esc back / q quit");
        return;
    }
    if (width < 44 || height < 17) {
        static const char* const lines[] = {
            "space / 1-4 pose", "n new kitty", "p pause / resume",
            "t palette", "h hide / show UI", "? / esc back", "q quit"
        };
        int y = (height - 8) / 2;
        int x = (width - 16) / 2;
        erase();
        style(cfg, CKCLR_FUR, 1);
        text_at(y, x, 16, "kitty controls");
        for (int i = 0; i < 7; i++) {
            style(cfg, i % 2 ? CKCLR_GRAY : CKCLR_PAW, 0);
            text_at(y + i + 1, x, 16, lines[i]);
        }
        return;
    }
    int box_w = width >= 58 ? 54 : width - 4;
    int x = (width - box_w) / 2;
    int y = (height - 15) / 2;
    style(cfg, CKCLR_GROUND, 0);
    for (int row = y; row < y + 15; row++) (void)mvhline(row, x, ' ', box_w);
    (void)mvhline(y, x, '-', box_w);
    (void)mvhline(y + 14, x, '-', box_w);
    style(cfg, CKCLR_FUR, 1);
    text_at(y + 1, x + 2, box_w - 4, "make yourself at home");
    static const char* const lines[] = {
        "space / 1-4   choose a pose", "n             meet a new kitty",
        "p             pause / resume", "t             change palette",
        "h             hide / show interface", "?             open / close help",
        "esc           back, then quit", "q             quit anytime"
    };
    for (int i = 0; i < 8; i++) {
        style(cfg, i % 2 ? CKCLR_GRAY : CKCLR_PAW, 0);
        text_at(y + 3 + i, x + 2, box_w - 4, lines[i]);
    }
    style(cfg, CKCLR_ACCENT, 0);
    text_at(y + 12, x + 2, box_w - 4, "take your time. kitty is waiting.");
}

/* Keep the complete animated silhouette inside its stage. Birds are optional
 * scenery and yield first when a short terminal needs room for the cat. */
static void place_kitty(ckitty_kitty* kitty, const Stage* stage, int walking_x) {
    int reach = kitty->pose == CKPOSE_PLAY ? 19 : kitty->pose == CKPOSE_WALK ? 13 : 11;
    int min_x = stage->left + reach;
    int max_x = stage->right - reach;
    kitty->cx = (stage->left + stage->right) / 2 + walking_x;
    if (min_x <= max_x) {
        if (kitty->cx < min_x) kitty->cx = min_x;
        if (kitty->cx > max_x) kitty->cx = max_x;
    }
    int depth = kitty->pose == CKPOSE_PLAY ? 5 : kitty->pose == CKPOSE_SLEEP ? 3 : 4;
    kitty->cy = stage->bottom - depth;
    int bird_top = kitty->cy + kitty->bird_dy - (kitty->pose == CKPOSE_PLAY ? 7 : 3);
    if (bird_top < stage->top) kitty->has_bird = 0;
}

static void draw_scene(const ckitty_canvas* canvas, const Stage* stage,
                       const Config* cfg, uint64_t frame, const ckitty_kitty* kitty) {
    style(cfg, CKCLR_GROUND, 0);
    int shadow_y = kitty->cy + (kitty->pose == CKPOSE_PLAY ? 5 :
                                kitty->pose == CKPOSE_SLEEP ? 4 : 5);
    int start = kitty->cx - 9;
    int end = kitty->cx + 9;
    for (int x = start; x <= end; x++) {
        if (x >= stage->left && x <= stage->right) (void)mvaddch(shadow_y, x, x % 2 ? '_' : '.');
    }
    int last_color = -1;
    for (int y = stage->top; y <= stage->bottom; y++) {
        for (int x = stage->left; x <= stage->right; x++) {
            char ch = ckitty_canvas_get(canvas, x, y);
            if (ch == ' ') continue;
            int color = ckitty_canvas_get_color(canvas, x, y);
            if (cfg->rainbow) color = (int)((frame / UINT64_C(10) + (uint64_t)x + (uint64_t)y) % UINT64_C(5)) + 1;
            if (color != last_color) {
                style(cfg, color, color == CKCLR_PAW);
                last_color = color;
            }
            (void)mvaddch(y, x, (chtype)(unsigned char)ch);
        }
    }
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
                        double* progress, int* grown, int restart) {
    if (!kitty || !full || !order || !progress || !grown) return 0;
    ckitty_draw_order_free(order);
    if (!prepare_live(kitty, full, order)) return 0;
    if (restart) {
        *progress = 0;
        *grown = 0;
    }
    return 1;
}

static void choose_kitty(ckitty_kitty* kitty, ckitty_rng* rng, int pose_override) {
    ckitty_kitty_randomize(kitty, rng, 0, 0);
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
        .has_seed = 0,
        .seed = 0,
        .message = NULL,
        .dump = 0,
        .dump_w = 80,
        .dump_h = 24,
        .dump_frame = 0,
        .theme = 0,
        .quiet = 0,
        .pose_override = -1
    };

    enum {
        OPT_DUMP = 1000,
        OPT_WIDTH,
        OPT_HEIGHT,
        OPT_FRAME,
        OPT_VERSION,
        OPT_THEME,
        OPT_QUIET
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
        {"theme", required_argument, 0, OPT_THEME},
        {"quiet", no_argument, 0, OPT_QUIET},
        {0, 0, 0, 0}
    };

    opterr = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "hcarlSis:d:g:p:m:", long_opts, NULL)) != -1) {
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
                break;
            case 'i':
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
            case OPT_THEME:
                cfg.theme = parse_theme(optarg);
                if (cfg.theme < 0) {
                    fprintf(stderr, "ckitty: invalid theme: %s (choose amber, moon, or forest)\n", optarg);
                    return 2;
                }
                break;
            case OPT_QUIET:
                cfg.quiet = 1;
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

    /* Keep --dump byte-for-byte stable; the interactive screen needs the
     * user's character widths before curses initializes its wide renderer. */
    (void)setlocale(LC_CTYPE, "");
    if (initscr() == NULL) {
        fprintf(stderr, "ckitty: could not initialize the terminal\n");
        return 1;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(50);
#ifdef NCURSES_VERSION
    set_escdelay(25);
#endif

    int width = 0;
    int height = 0;
    getmaxyx(stdscr, height, width);
    if (width <= 0 || height <= 0) {
        endwin();
        fprintf(stderr, "ckitty: terminal has no usable rows or columns\n");
        return 1;
    }

    if (cfg.colors && !cfg.ascii && has_colors()) {
        if (start_color() == ERR || !apply_theme(cfg.theme)) {
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
    choose_kitty(&kitty, &rng, cfg.pose_override);

    ckitty_draw_order order = {0};
    /* Store a fraction so layout changes, including zero-cell tiny canvases,
     * preserve the same reveal progress. Only a new subject starts over. */
    double live_progress = 0;
    int grown = cfg.live ? 0 : 1;
    Stage stage = stage_layout(width, height, &cfg);
    ckitty_kitty placed = kitty;
    place_kitty(&placed, &stage, 0);
    if (cfg.live && !rebuild_live(&placed, &live_full, &order, &live_progress, &grown, 1)) {
        ckitty_canvas_free(&canvas);
        ckitty_canvas_free(&live_full);
        ckitty_canvas_free(&live_visible_canvas);
        endwin();
        fprintf(stderr, "ckitty: could not prepare live rendering\n");
        return 1;
    }

    uint64_t next_spawn = now_ms() + SCREENSAVER_PERIOD_MS;
    uint64_t next_frame = now_ms();
    uint64_t frame = 0;
    uint64_t frozen_at = 0;
    int paused = 0;
    int help = 0;
    int walking_x = 0;
    int rebuild = 0;
    int restart_reveal = 0;
    int exit_code = 0;
    int running = 1;
    int first_frame = 1;
    int redraw = 1;

    /* Input wakes the loop even at very slow animation speeds. Capping the
     * timeout also lets resize signals be handled while motion is paused. */
    while (running) {
        int new_width = 0;
        int new_height = 0;
        getmaxyx(stdscr, new_height, new_width);
        if (new_width != width || new_height != height) {
            if (!resize_canvas(&canvas, new_width, new_height) ||
                (cfg.live && !resize_live_canvases(&live_full, &live_visible_canvas,
                                                  new_width, new_height))) {
                exit_code = 1;
                break;
            }
            width = new_width;
            height = new_height;
            rebuild = 1;
        }
        int was_growing = !stage.small && cfg.live && !grown;
        stage = stage_layout(width, height, &cfg);
        uint64_t now = now_ms();
        int is_growing = !stage.small && cfg.live && !grown;
        /* Compact kitties are already complete. A layout switch must not
         * inherit a long deadline from the other animation's timing option. */
        if (was_growing != is_growing) next_frame = now;
        int frozen = paused || help || width < 20 || height < 8;
        if (frozen && !frozen_at) frozen_at = now;
        if (!frozen && frozen_at) {
            next_spawn += now - frozen_at;
            next_frame = now;
            frozen_at = 0;
        }
        if (!frozen && cfg.screensaver && now >= next_spawn) {
            seed = ckitty_rng_u32(&rng);
            ckitty_rng_seed(&rng, seed);
            choose_kitty(&kitty, &rng, cfg.pose_override);
            frame = 0;
            first_frame = 1;
            walking_x = 0;
            next_spawn = now + SCREENSAVER_PERIOD_MS;
            next_frame = now;
            rebuild = 1;
            restart_reveal = 1;
        }
        int tick = !frozen && now >= next_frame;
        if (tick && !first_frame) frame++;
        if (tick || rebuild || redraw) {
            placed = kitty;
            place_kitty(&placed, &stage, walking_x);
            if (rebuild && cfg.live) {
                if (!rebuild_live(&placed, &live_full, &order, &live_progress, &grown,
                                  restart_reveal)) {
                    exit_code = 1;
                    break;
                }
            }
            rebuild = 0;
            restart_reveal = 0;
            if (tick && grown && kitty.pose == CKPOSE_WALK && frame % 3ULL == 0) {
                int reach = 13;
                int travel = (stage.right - stage.left) / 2 - reach;
                if (travel > 0) {
                    walking_x += kitty.facing;
                    if (walking_x >= travel) { walking_x = travel; kitty.facing = -1; }
                    if (walking_x <= -travel) { walking_x = -travel; kitty.facing = 1; }
                } else walking_x = 0;
                placed = kitty;
                place_kitty(&placed, &stage, walking_x);
            }
            const ckitty_canvas* to_draw = &canvas;
            if (!stage.small && cfg.live && !grown) {
                if (tick) {
                    live_progress += order.len > 0 ?
                        (double)(order.len / 80 + 1) / (double)order.len : 1.0;
                    if (live_progress >= 1.0) { live_progress = 1.0; grown = 1; }
                }
                int live_visible = (int)(live_progress * (double)order.len);
                ckitty_canvas_copy_visible(&live_full, &order, live_visible, &live_visible_canvas);
                to_draw = &live_visible_canvas;
            } else if (!stage.small) ckitty_render_frame(&placed, frame, &canvas);

            erase();
            if (stage.small) draw_small_kitty(width, height, &cfg, kitty.pose, frame);
            else {
                draw_ambient(&stage, &cfg, kitty.seed, frame);
                draw_scene(to_draw, &stage, &cfg, frame, &placed);
            }
            draw_chrome(width, height, &cfg, &kitty, seed, paused,
                        !stage.small && cfg.live && !grown);
            if (help) draw_help(width, height, &cfg);
            attrset(A_NORMAL);
            refresh();
            redraw = 0;
        }

        if (tick) {
            first_frame = 0;
            int delay = !stage.small && cfg.live && !grown ? cfg.grow_delay_us : cfg.delay_us;
            next_frame = now + ((uint64_t)delay + UINT64_C(999)) / UINT64_C(1000);
        }
        uint64_t wait = frozen || next_frame <= now ? 50ULL : next_frame - now;
        timeout((int)(wait > 50ULL ? 50ULL : wait));
        int input = getch();
        redraw = input != ERR;
        if (input == 'q') running = 0;
        else if (input == 27) { if (help) help = 0; else running = 0; }
        else if (input == '?') help = !help;
        else if (!help) {
            if (input == 'p') paused = !paused;
            else if (input == 'h') { cfg.quiet = !cfg.quiet; rebuild = 1; }
            else if (input == 't') {
                cfg.theme = (cfg.theme + 1) % THEME_COUNT;
                if (cfg.colors && !apply_theme(cfg.theme)) cfg.colors = 0;
            } else if (input == ' ' || (input >= '1' && input <= '4')) {
                kitty.pose = input == ' ' ? (ckitty_pose)(((int)kitty.pose + 1) % 4) :
                                            (ckitty_pose)(input - '1');
                frame = 0;
                first_frame = 1;
                walking_x = 0;
                next_frame = now_ms();
                rebuild = 1;
                restart_reveal = 1;
            } else if (input == 'n') {
                seed = ckitty_rng_u32(&rng);
                ckitty_rng_seed(&rng, seed);
                choose_kitty(&kitty, &rng, cfg.pose_override);
                frame = 0;
                first_frame = 1;
                walking_x = 0;
                uint64_t reset_at = now_ms();
                next_spawn = reset_at + SCREENSAVER_PERIOD_MS;
                if (frozen_at) frozen_at = reset_at;
                next_frame = reset_at;
                rebuild = 1;
                restart_reveal = 1;
            }
        }
    }

    ckitty_draw_order_free(&order);
    ckitty_canvas_free(&canvas);
    ckitty_canvas_free(&live_full);
    ckitty_canvas_free(&live_visible_canvas);
    endwin();
    return exit_code;
}
