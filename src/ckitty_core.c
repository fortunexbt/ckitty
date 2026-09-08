#include "ckitty_core.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t xorshift32(uint32_t* state) {
    // xorshift32: state must be non-zero.
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void ckitty_rng_seed(ckitty_rng* rng, uint32_t seed) {
    if (!rng) return;
    rng->state = (seed == 0) ? 0x1u : seed;
}

uint32_t ckitty_rng_u32(ckitty_rng* rng) {
    if (!rng) return 0;
    if (rng->state == 0) rng->state = 0x1u;
    return xorshift32(&rng->state);
}

int ckitty_rng_range(ckitty_rng* rng, int max_exclusive) {
    if (max_exclusive <= 0) return 0;
    return (int)(ckitty_rng_u32(rng) % (uint32_t)max_exclusive);
}

int ckitty_rng_percent(ckitty_rng* rng) {
    return ckitty_rng_range(rng, 100);
}

static size_t canvas_size_bytes(int w, int h) {
    if (w <= 0 || h <= 0) return 0;
    if ((size_t)w > SIZE_MAX / (size_t)h) return 0;
    if ((uint64_t)(size_t)w * (uint64_t)(size_t)h > CKITTY_MAX_CANVAS_CELLS) return 0;
    return (size_t)w * (size_t)h;
}

int ckitty_canvas_init(ckitty_canvas* c, int w, int h) {
    if (!c) return 0;
    if (c->ch || c->color) return 0;
    if (w <= 0 || h <= 0) return 0;

    size_t n = canvas_size_bytes(w, h);
    if (n == 0) return 0;

    c->w = w;
    c->h = h;
    c->ch = (char*)malloc(n);
    c->color = (uint8_t*)malloc(n);
    if (!c->ch || !c->color) {
        free(c->ch);
        free(c->color);
        c->ch = NULL;
        c->color = NULL;
        c->w = 0;
        c->h = 0;
        return 0;
    }

    memset(c->ch, ' ', n);
    memset(c->color, 0, n);
    return 1;
}

void ckitty_canvas_free(ckitty_canvas* c) {
    if (!c) return;
    free(c->ch);
    free(c->color);
    c->ch = NULL;
    c->color = NULL;
    c->w = 0;
    c->h = 0;
}

void ckitty_canvas_clear(ckitty_canvas* c, char fill) {
    if (!c || !c->ch || !c->color) return;
    size_t n = canvas_size_bytes(c->w, c->h);
    memset(c->ch, fill, n);
    memset(c->color, 0, n);
}

void ckitty_canvas_set(ckitty_canvas* c, int x, int y, char ch, uint8_t color) {
    if (!c || !c->ch || !c->color) return;
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    size_t idx = (size_t)y * (size_t)c->w + (size_t)x;
    c->ch[idx] = ch;
    c->color[idx] = (ch == ' ') ? 0 : color;
}

char ckitty_canvas_get(const ckitty_canvas* c, int x, int y) {
    if (!c || !c->ch) return ' ';
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return ' ';
    size_t idx = (size_t)y * (size_t)c->w + (size_t)x;
    return c->ch[idx];
}

uint8_t ckitty_canvas_get_color(const ckitty_canvas* c, int x, int y) {
    if (!c || !c->color) return 0;
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return 0;
    size_t idx = (size_t)y * (size_t)c->w + (size_t)x;
    return c->color[idx];
}

char* ckitty_canvas_dump_bbox(const ckitty_canvas* c) {
    if (!c || !c->ch || c->w <= 0 || c->h <= 0) {
        char* s = (char*)malloc(1);
        if (s) s[0] = '\0';
        return s;
    }

    int min_x = c->w;
    int min_y = c->h;
    int max_x = -1;
    int max_y = -1;

    for (int y = 0; y < c->h; y++) {
        for (int x = 0; x < c->w; x++) {
            if (ckitty_canvas_get(c, x, y) != ' ') {
                if (x < min_x) min_x = x;
                if (y < min_y) min_y = y;
                if (x > max_x) max_x = x;
                if (y > max_y) max_y = y;
            }
        }
    }

    if (max_x < min_x || max_y < min_y) {
        char* s = (char*)malloc(1);
        if (s) s[0] = '\0';
        return s;
    }

    int out_w = (max_x - min_x) + 1;
    int out_h = (max_y - min_y) + 1;
    size_t line_len = (size_t)out_w;
    size_t total = (line_len + 1) * (size_t)out_h + 1;

    char* out = (char*)malloc(total);
    if (!out) return NULL;

    size_t off = 0;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            out[off++] = ckitty_canvas_get(c, x, y);
        }
        out[off++] = '\n';
    }
    out[off] = '\0';
    return out;
}

static void put(ckitty_canvas* c, int x, int y, char ch, uint8_t color) {
    ckitty_canvas_set(c, x, y, ch, color);
}

static void put_str(ckitty_canvas* c, int x, int y, const char* s, uint8_t color) {
    if (!s) return;
    for (int i = 0; s[i] != '\0'; i++) {
        put(c, x + i, y, s[i], color);
    }
}

static void render_tail(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c, int base_x, int base_y, int dir) {
    if (!k || !c) return;
    int len = (k->tail_len < 1) ? 1 : k->tail_len;
    double phase = k->tail_phase;
    double amp = k->tail_amp;
    double speed = 0.11;
    int curve_start = (len * 2) / 3;
    int previous_y = base_y;
    int wag = (int)((frame / 8ULL) % 3ULL) - 1;

    for (int i = 0; i < len; i++) {
        double t = (len <= 1) ? 0.0 : ((double)i / (double)(len - 1));
        double sway = sin((double)frame * speed + phase + t * 2.2) * amp * 0.55;
        int x = base_x + dir * i;
        int y = base_y + wag + (int)lrint(sway);
        if (i >= curve_start) {
            // Keep the path connected so the tail reads as a gesture, not dots.
            int hook = i - curve_start + 1;
            y -= (hook + 1) / 2;
        }
        if (i == 0) y = base_y;
        if (y > previous_y + 1) y = previous_y + 1;
        if (y < previous_y - 1) y = previous_y - 1;

        int dy = y - previous_y;
        char ch = '~';
        if (dy != 0) ch = (dy * dir > 0) ? '\\' : '/';
        else if (i > 0 && t > 0.88) ch = '.';

        put(c, x, y, ch, CKCLR_FUR);
        previous_y = y;
    }
}

static char open_eye_char(int eye_style) {
    switch (eye_style % 3) {
        case 0:
            return 'o';
        case 1:
            return 'O';
        default:
            return '.';
    }
}

static char mouth_char(int mouth_style) {
    switch (mouth_style % 3) {
        case 0:
            return 'w';
        case 1:
            return '3';
        default:
            return 'v';
    }
}

static int eye_state(const ckitty_kitty* k, uint64_t frame) {
    if (!k) return 0;
    if (k->pose == CKPOSE_SLEEP) return 2;  // sleep

    // Blink rarely and deterministically per kitty.
    int base = 90 + (int)(k->seed % 60u);  // 90..149
    int t = (base > 0) ? (int)(frame % (uint64_t)base) : 0;
    return (t == 1 || t == 2) ? 1 : 0;  // 1=blink, 0=open
}

static int whisker_twitch(const ckitty_kitty* k, uint64_t frame) {
    if (!k) return 0;
    int period = 50 + (int)(k->seed % 40u);  // 50..89
    int t = (period > 0) ? (int)(frame % (uint64_t)period) : 0;
    return (t > 0 && t < 5) ? 1 : 0;
}

static void render_head(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c, int x0, int y0) {
    int es = eye_state(k, frame);
    char eye_l = open_eye_char(k->eye_style);
    char eye_r = eye_l;
    if (es == 1) {
        eye_l = '-';
        eye_r = '-';
    } else if (es == 2) {
        eye_l = '^';
        eye_r = '^';
    }

    char mouth = mouth_char(k->mouth_style);

    // Ears / top.
    put_str(c, x0, y0, " /\\_/\\ ", CKCLR_FUR);

    // Face: "( o.o )". Symmetry makes the tiny face read at a glance.
    put(c, x0 + 0, y0 + 1, '(', CKCLR_FUR);
    put(c, x0 + 1, y0 + 1, ' ', CKCLR_FUR);
    put(c, x0 + 2, y0 + 1, eye_l, CKCLR_PAW);
    put(c, x0 + 3, y0 + 1, '.', CKCLR_NOSE);
    put(c, x0 + 4, y0 + 1, eye_r, CKCLR_PAW);
    put(c, x0 + 5, y0 + 1, ' ', CKCLR_FUR);
    put(c, x0 + 6, y0 + 1, ')', CKCLR_FUR);

    // Mouth: " > w < "
    put(c, x0 + 0, y0 + 2, ' ', CKCLR_FUR);
    put(c, x0 + 1, y0 + 2, '>', CKCLR_PAW);
    put(c, x0 + 2, y0 + 2, ' ', CKCLR_FUR);
    put(c, x0 + 3, y0 + 2, mouth, CKCLR_NOSE);
    put(c, x0 + 4, y0 + 2, ' ', CKCLR_FUR);
    put(c, x0 + 5, y0 + 2, '<', CKCLR_PAW);
    put(c, x0 + 6, y0 + 2, ' ', CKCLR_FUR);

    // A twitch swaps the staggered rows within the face, never onto the body.
    int wt = whisker_twitch(k, frame);
    int wy = y0 + 1 + wt;
    int other_wy = y0 + 2 - wt;
    put(c, x0 - 3, wy, '-', CKCLR_GRAY);
    put(c, x0 - 2, wy, '-', CKCLR_GRAY);
    put(c, x0 + 7, wy, '-', CKCLR_GRAY);
    put(c, x0 + 8, wy, '-', CKCLR_GRAY);
    put(c, x0 - 2, other_wy, '-', CKCLR_GRAY);
    put(c, x0 - 1, other_wy, '-', CKCLR_GRAY);
    put(c, x0 + 7, other_wy, '-', CKCLR_GRAY);
    put(c, x0 + 8, other_wy, '-', CKCLR_GRAY);
}

static void render_body_box(const ckitty_kitty* k, ckitty_canvas* c, int x0, int y0, ckitty_rng* rng) {
    int w = (k->body_w < 9) ? 9 : k->body_w;
    int h = (k->body_h < 3) ? 3 : k->body_h;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            char ch = ' ';
            if (y == 0) {
                if (x == 0 || x == w - 1) ch = '.';
                else if (x == 1 || x == w - 2) ch = '-';
                else ch = '~';
            } else if (y == h - 1) {
                if (x == 0) ch = '\\';
                else if (x == w - 1) ch = '/';
                else ch = '_';
            } else if (x == 0) ch = '|';
            else if (x == w - 1) ch = '|';
            else {
                // Stable fur fill.
                if (ckitty_rng_percent(rng) < k->fur_density) {
                    ch = (ckitty_rng_range(rng, 2) == 0) ? k->fur_a : k->fur_b;
                } else {
                    // Still advance RNG consistently across frames.
                    (void)ckitty_rng_u32(rng);
                }
            }
            if (ch != ' ') put(c, x0 + x, y0 + y, ch, CKCLR_FUR);
        }
    }
}

static void render_paws(ckitty_canvas* c, int x_left, int x_right, int y) {
    put_str(c, x_left, y, "(_)", CKCLR_PAW);
    put_str(c, x_right, y, "(_)", CKCLR_PAW);
}

static void render_yarn_ball(ckitty_canvas* c, int x, int y) {
    put(c, x - 1, y - 1, '/', CKCLR_TOY);
    put(c, x, y - 1, '@', CKCLR_TOY);
    put(c, x + 1, y - 1, '\\', CKCLR_TOY);
    put(c, x - 1, y, '@', CKCLR_TOY);
    put(c, x, y, '@', CKCLR_TOY);
    put(c, x + 1, y, '@', CKCLR_TOY);
    put(c, x, y + 1, '@', CKCLR_TOY);

    // string
    for (int i = 0; i < 5; i++) {
        put(c, x + 2 + i, y + (i % 2), '~', CKCLR_TOY);
    }
}

static void render_mouse(ckitty_canvas* c, int x, int y) {
    put_str(c, x, y, "<:3~~", CKCLR_GRAY);
}

static void render_bird(ckitty_canvas* c, int x, int y) {
    put_str(c, x, y, "v^v", CKCLR_ACCENT);
}

static void render_sitting(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    ckitty_rng rng;
    ckitty_rng_seed(&rng, k->seed ^ 0x9e3779b9u);

    int body_w = (k->body_w < 9) ? 9 : k->body_w;
    int body_h = (k->body_h < 3) ? 3 : k->body_h;

    int body_x0 = k->cx - body_w / 2;
    int body_y0 = k->cy;

    // Tail behind the body (back side).
    int base_x = (k->facing > 0) ? (body_x0 - 1) : (body_x0 + body_w);
    int base_y = body_y0 + body_h - 1;
    int tail_dir = (k->facing > 0) ? -1 : 1;
    render_tail(k, frame, c, base_x, base_y, tail_dir);

    // Body.
    render_body_box(k, c, body_x0, body_y0, &rng);

    // Head.
    int head_x0 = k->cx - 3;
    int head_y0 = body_y0 - 3;
    render_head(k, frame, c, head_x0, head_y0);

    // Paws under body.
    int paws_y = body_y0 + body_h;
    render_paws(c, body_x0 + 1, body_x0 + body_w - 4, paws_y);

    // Environment.
    if (k->has_bird) {
        render_bird(c, k->cx + k->bird_dx, head_y0 + k->bird_dy);
    }
}

static void render_sleeping(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    int cx = k->cx;
    int cy = k->cy;

    // A deliberate curled-up silhouette keeps the sleepy pose readable at a
    // glance; the fur texture still varies deterministically inside it.
    ckitty_rng rng;
    ckitty_rng_seed(&rng, k->seed ^ 0x85ebca6bu);
    put_str(c, cx - 6, cy, " .-~~~~~~-. ", CKCLR_FUR);
    put_str(c, cx - 7, cy + 1, "/  .    .  \\", CKCLR_FUR);
    put_str(c, cx - 6, cy + 2, "\\____  ____/", CKCLR_FUR);
    put_str(c, cx - 4, cy + 3, "  (____)  ", CKCLR_FUR);
    for (int i = -3; i <= 3; i++) {
        if (ckitty_rng_percent(&rng) < 45) {
            put(c, cx + i, cy + 1, (i % 2 == 0) ? '.' : '~', CKCLR_FUR);
        }
    }

    ckitty_kitty face = *k;
    face.pose = CKPOSE_SLEEP;
    render_head(&face, frame, c, cx - 3, cy - 3);

    // Z's drift on a slow, deterministic loop.
    int period = 40 + (int)(k->seed % 20u);
    int t = (period > 0) ? (int)(frame % (uint64_t)period) : 0;
    if (t < period / 2) {
        put(c, cx + 8, cy - 3 - (t / 20), 'z', CKCLR_ACCENT);
        put(c, cx + 9, cy - 4 - (t / 20), 'Z', CKCLR_ACCENT);
    }

    if (k->has_bird) {
        render_bird(c, cx + k->bird_dx, cy + k->bird_dy - 2);
    }
}

static void render_playing(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    ckitty_rng rng;
    ckitty_rng_seed(&rng, k->seed ^ 0xc2b2ae35u);

    int dir = (k->facing >= 0) ? 1 : -1;
    int cx = k->cx;
    int bounce_phase = (int)((frame / 10ULL) % 4ULL);
    int cy = k->cy + ((bounce_phase == 1) ? 1 : ((bounce_phase == 3) ? -1 : 0));

    // Stretch body: use the same rounded silhouette as the sitting pose so
    // the character remains a cat while its stance changes.
    int body_len = 9 + (k->body_w - 7);
    int body_left = cx - body_len / 2;
    int body_right = body_left + body_len - 1;
    ckitty_kitty body = *k;
    body.body_w = body_len;
    body.body_h = 3;
    render_body_box(&body, c, body_left, cy + 1, &rng);

    // Paws up front, reaching toward the toy. Keep them below the face so
    // the pounce reads as one connected cat instead of face/paw collisions.
    if (dir > 0) {
        put_str(c, body_right + 1, cy + 1, "/__\\", CKCLR_PAW);
        put(c, body_right + 3, cy + 2, '|', CKCLR_PAW);
    } else {
        put_str(c, body_left - 4, cy + 1, "\\__/", CKCLR_PAW);
        put(c, body_left - 3, cy + 2, '|', CKCLR_PAW);
    }

    // Excited tail behind.
    int tail_base_x = (dir > 0) ? body_left - 1 : body_right + 1;
    int tail_base_y = cy + 3;
    render_tail(k, frame, c, tail_base_x, tail_base_y, -dir);

    // Head at the front.
    int head_x0 = (dir > 0) ? (body_right - 3) : (body_left - 3);
    int head_y0 = cy - 2;
    render_head(k, frame, c, head_x0, head_y0);

    // One toy at a time keeps the ball and mouse silhouettes distinct.
    if (k->has_yarn) {
        int yx = (dir > 0) ? (body_right + 11) : (body_left - 11);
        int toy_bob = ((int)(frame / 12ULL) % 3 == 1) ? -1 : 0;
        render_yarn_ball(c, yx, cy + 2 + toy_bob);
    } else if (k->has_mouse) {
        int mx = (dir > 0) ? (body_right + 10) : (body_left - 10);
        render_mouse(c, mx, cy + 3 + ((int)(frame / 12ULL) % 3 == 1 ? -1 : 0));
    }
    if (k->has_bird) {
        render_bird(c, cx + k->bird_dx, cy - 6 + k->bird_dy);
    }
}

static void render_walking(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    ckitty_rng rng;
    ckitty_rng_seed(&rng, k->seed ^ 0x27d4eb2fu);

    int body_w = (k->body_w < 9) ? 9 : k->body_w;
    int body_h = (k->body_h < 3) ? 3 : k->body_h;

    int body_x0 = k->cx - body_w / 2;
    int body_y0 = k->cy;

    // Tail behind.
    int base_x = (k->facing > 0) ? (body_x0 - 1) : (body_x0 + body_w);
    int base_y = body_y0 + body_h - 1;
    int tail_dir = (k->facing > 0) ? -1 : 1;
    render_tail(k, frame, c, base_x, base_y, tail_dir);

    // Body.
    render_body_box(k, c, body_x0, body_y0, &rng);

    // Head slightly forward.
    int head_x0 = (k->facing > 0) ? (k->cx - 2) : (k->cx - 4);
    int head_y0 = body_y0 - 3;
    render_head(k, frame, c, head_x0, head_y0);

    // Walking paws alternate.
    int paws_y = body_y0 + body_h;
    if ((frame / 8) % 2 == 0) {
        render_paws(c, body_x0 + 0, body_x0 + body_w - 3, paws_y);
    } else {
        render_paws(c, body_x0 + 2, body_x0 + body_w - 5, paws_y);
    }

    if (k->has_bird) {
        render_bird(c, k->cx + k->bird_dx, head_y0 + k->bird_dy);
    }
}

void ckitty_kitty_randomize(ckitty_kitty* k, ckitty_rng* rng, int cx, int cy) {
    if (!k || !rng) return;

    memset(k, 0, sizeof(*k));

    k->seed = ckitty_rng_u32(rng);
    if (k->seed == 0) k->seed = 0xA341316Cu;

    k->pose = (ckitty_pose)ckitty_rng_range(rng, 4);
    k->facing = (ckitty_rng_range(rng, 2) == 0) ? -1 : 1;
    k->cx = cx;
    k->cy = cy;

    k->body_w = 9 + 2 * ckitty_rng_range(rng, 3);  // 9,11,13
    k->body_h = 3 + ckitty_rng_range(rng, 2);      // 3..4
    k->fur_density = 45 + ckitty_rng_range(rng, 28);

    static const char fur_chars[] = {'*', '.', ':', 'o', '+', '\''};
    k->fur_a = fur_chars[ckitty_rng_range(rng, (int)sizeof(fur_chars))];
    k->fur_b = fur_chars[ckitty_rng_range(rng, (int)sizeof(fur_chars))];

    k->tail_len = 10 + ckitty_rng_range(rng, 8);
    k->tail_amp = 0.8 + (double)ckitty_rng_range(rng, 10) / 10.0;
    k->tail_phase = (double)ckitty_rng_range(rng, 628) / 100.0;  // 0..6.28

    k->eye_style = ckitty_rng_range(rng, 3);
    k->mouth_style = ckitty_rng_range(rng, 3);

    k->has_yarn = (ckitty_rng_percent(rng) < 55) ? 1 : 0;
    k->has_mouse = (ckitty_rng_percent(rng) < 35) ? 1 : 0;
    k->has_bird = (ckitty_rng_percent(rng) < 25) ? 1 : 0;
    k->bird_dx = ckitty_rng_range(rng, 21) - 10;
    k->bird_dy = -(3 + ckitty_rng_range(rng, 3));
}

void ckitty_render_frame(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* out) {
    if (!k || !out) return;
    ckitty_canvas_clear(out, ' ');

    switch (k->pose) {
        case CKPOSE_SIT:
            render_sitting(k, frame, out);
            break;
        case CKPOSE_SLEEP:
            render_sleeping(k, frame, out);
            break;
        case CKPOSE_PLAY:
            render_playing(k, frame, out);
            break;
        case CKPOSE_WALK:
        default:
            render_walking(k, frame, out);
            break;
    }
}

void ckitty_draw_order_free(ckitty_draw_order* o) {
    if (!o) return;
    free(o->idx);
    o->idx = NULL;
    o->len = 0;
    o->cap = 0;
}

static int draw_order_reserve(ckitty_draw_order* o, int cap) {
    if (!o) return 0;
    if (cap <= o->cap) return 1;
    int new_cap = o->cap ? o->cap : 64;
    while (new_cap < cap) new_cap *= 2;
    int* p = (int*)realloc(o->idx, (size_t)new_cap * sizeof(int));
    if (!p) return 0;
    o->idx = p;
    o->cap = new_cap;
    return 1;
}

int ckitty_draw_order_build(const ckitty_canvas* full, uint32_t seed, ckitty_draw_order* out) {
    if (!full || !full->ch || !full->color || full->w <= 0 || full->h <= 0 || !out) return 0;

    out->len = 0;
    // Count non-space cells.
    int count = 0;
    for (int y = 0; y < full->h; y++) {
        for (int x = 0; x < full->w; x++) {
            if (ckitty_canvas_get(full, x, y) != ' ') count++;
        }
    }
    if (count == 0) return 1;
    if (!draw_order_reserve(out, count)) return 0;

    for (int y = 0; y < full->h; y++) {
        for (int x = 0; x < full->w; x++) {
            if (ckitty_canvas_get(full, x, y) != ' ') {
                out->idx[out->len++] = y * full->w + x;
            }
        }
    }

    // Deterministic shuffle.
    ckitty_rng rng;
    ckitty_rng_seed(&rng, seed ^ 0xDEADBEEFu);
    for (int i = out->len - 1; i > 0; i--) {
        int j = ckitty_rng_range(&rng, i + 1);
        int tmp = out->idx[i];
        out->idx[i] = out->idx[j];
        out->idx[j] = tmp;
    }
    return 1;
}

void ckitty_canvas_copy_visible(const ckitty_canvas* full, const ckitty_draw_order* order, int visible, ckitty_canvas* out) {
    if (!full || !full->ch || !full->color || !order || !out || !out->ch || !out->color) return;
    if (full->w != out->w || full->h != out->h) return;
    ckitty_canvas_clear(out, ' ');
    if (visible < 0) visible = order->len;
    if (visible > order->len) visible = order->len;

    for (int i = 0; i < visible; i++) {
        int idx = order->idx[i];
        if (idx < 0 || (size_t)idx >= (size_t)full->w * (size_t)full->h) continue;
        out->ch[idx] = full->ch[idx];
        out->color[idx] = full->color[idx];
    }
}
