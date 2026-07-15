#ifndef CKITTY_CORE_H
#define CKITTY_CORE_H

#include <stdint.h>
#include <stddef.h>

// Core (non-ncurses) primitives used by both the terminal app and tests.

#define CKITTY_MAX_CANVAS_CELLS 4000000U

// Internal color indices (renderer maps these to curses color pairs).
enum {
    CKCLR_NONE = 0,
    CKCLR_FUR = 1,
    CKCLR_PAW = 2,
    CKCLR_NOSE = 3,
    CKCLR_TOY = 4,
    CKCLR_ACCENT = 5,
    CKCLR_GRAY = 6,
    CKCLR_GROUND = 7
};

typedef struct {
    uint32_t state;
} ckitty_rng;

void ckitty_rng_seed(ckitty_rng* rng, uint32_t seed);
uint32_t ckitty_rng_u32(ckitty_rng* rng);
int ckitty_rng_range(ckitty_rng* rng, int max_exclusive);
int ckitty_rng_percent(ckitty_rng* rng);

typedef enum {
    CKPOSE_SIT = 0,
    CKPOSE_SLEEP = 1,
    CKPOSE_PLAY = 2,
    CKPOSE_WALK = 3
} ckitty_pose;

typedef struct {
    int w;
    int h;
    char* ch;        // w*h, row-major, space-filled
    uint8_t* color;  // w*h, row-major, 0 means "no color"
} ckitty_canvas;

// Initialize a zeroed canvas. Call ckitty_canvas_free() before reusing it.
int ckitty_canvas_init(ckitty_canvas* c, int w, int h);
void ckitty_canvas_free(ckitty_canvas* c);
void ckitty_canvas_clear(ckitty_canvas* c, char fill);
void ckitty_canvas_set(ckitty_canvas* c, int x, int y, char ch, uint8_t color);
char ckitty_canvas_get(const ckitty_canvas* c, int x, int y);
uint8_t ckitty_canvas_get_color(const ckitty_canvas* c, int x, int y);

// Returns a newly allocated string containing the minimal bounding box of
// non-space characters. Caller frees().
char* ckitty_canvas_dump_bbox(const ckitty_canvas* c);

typedef struct {
    uint32_t seed;     // Used for stable (per-kitty) procedural variation.
    ckitty_pose pose;
    int facing;        // -1 left, +1 right
    int cx;            // anchor x (terminal coordinates)
    int cy;            // anchor y (terminal coordinates)

    // Visual parameters (stable for the kitty).
    int body_w;        // 7..11
    int body_h;        // 3..5
    int fur_density;   // 0..100
    char fur_a;
    char fur_b;
    int tail_len;      // 8..20
    double tail_amp;   // sway amplitude
    double tail_phase; // 0..2pi

    int eye_style;     // affects open-eye character
    int mouth_style;   // affects mouth character

    // Environment (stable for the kitty).
    int has_yarn;
    int has_mouse;
    int has_bird;
    int bird_dx;
    int bird_dy;
} ckitty_kitty;

void ckitty_kitty_randomize(ckitty_kitty* k, ckitty_rng* rng, int cx, int cy);
void ckitty_render_frame(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* out);

typedef struct {
    int* idx;  // indices into canvas (y*w + x)
    int len;
    int cap;
} ckitty_draw_order;

void ckitty_draw_order_free(ckitty_draw_order* o);
int ckitty_draw_order_build(const ckitty_canvas* full, uint32_t seed, ckitty_draw_order* out);
void ckitty_canvas_copy_visible(const ckitty_canvas* full, const ckitty_draw_order* order, int visible, ckitty_canvas* out);

#endif  // CKITTY_CORE_H
