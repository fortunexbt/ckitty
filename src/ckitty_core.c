#include "ckitty_core.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

/* Authored contours keep the ears, cheeks, shoulders, haunches and paws
 * connected. Seeded expressions and sparse coat marks vary within them. */
static const char* const sit_shape[] = {
    "     /\\___/\\",
    "    ( o   o )",
    "   =\\   ^   /=",
    "     )`---'(",
    "    /  | |  \\",
    "   (   | |   )__",
    "    \\__|_|__/   )",
    "     (__)(__)-'",
};

static const char* const sleep_shape[] = {
    "    /\\___/\\",
    "   ( -   - )____",
    "  =\\   ^   /    `.",
    "    `-----' /      \\",
    "   (_______/   __  )",
    "    \\        (  )/",
    "     `-.___..'--'",
};

static const char* const play_shape[] = {
    " __      _..---.._",
    "(  `-._.'        /\\___/\\",
    " `-.            ( o   o )",
    "    )     __   =\\   ^   /=",
    "   (    .'  `-.  `-----'\\",
    "    \\  (      `----(_____)",
    "     \\__)        (____)",
};

static const char* const walk_shape[] = {
    "  __     _..---.._",
    " (  \\_.-'       /\\___/\\",
    "  \\            ( o   o )",
    "   )   .----. =\\   ^   /=",
    "  /   /     /   `-----'\\",
    " (   /    _/   /   \\   )",
    "  \\__)   (____/    (___/",
};


static char mirrored_char(char ch) {
    switch (ch) {
        case '/': return '\\';
        case '\\': return '/';
        case '(': return ')';
        case ')': return '(';
        case '`': return '\'';
        case '\'': return '`';
        default: return ch;
    }
}

static void sprite_put(const ckitty_kitty* k, ckitty_canvas* c, int width,
                       int top, int x, int y, char ch, uint8_t color) {
    if (k->facing < 0) {
        x = width - 1 - x;
        ch = mirrored_char(ch);
    }
    put(c, k->cx - width / 2 + x, top + y, ch, color);
}

static void render_shape(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c,
                         const char* const* rows, int height, int top) {
    int width = 0;
    for (int y = 0; y < height; y++) {
        int length = (int)strlen(rows[y]);
        if (length > width) width = length;
    }
    int blink_period = 90 + (int)(k->seed % 60U);
    uint64_t blink = frame % (uint64_t)blink_period;
    char eye = (k->pose == CKPOSE_SLEEP || blink == 1 || blink == 2) ? '-' :
               k->eye_style == 1 ? 'O' : 'o';
    int twitch = frame % (uint64_t)(50 + k->seed % 40U) < 4;
    uint64_t tail_phase = (frame / 18U) % 2U;

    for (int y = 0; y < height; y++) {
        const char* line = rows[y];
        /* Short, alternating paw placements give the low walk a soft step. */
        if (k->pose == CKPOSE_WALK && (frame / 10U) % 2U) {
            if (y == height - 2) line = " (   /     \\  /    /  )";
            if (y == height - 1) line = "  \\___)    (__)   (___/";
        }
        for (int x = 0; line[x]; x++) {
            char ch = line[x];
            if (ch == ' ') continue;
            uint8_t color = CKCLR_FUR;
            if (ch == 'o') { ch = eye; color = CKCLR_PAW; }
            else if (ch == '^') color = CKCLR_NOSE;
            else if (ch == '=') { ch = twitch ? '-' : '='; color = CKCLR_GRAY; }
            else if (k->pose == CKPOSE_SIT && ch == '|') color = CKCLR_PAW;
            else if ((k->pose == CKPOSE_SIT && y == height - 1 && x >= 5 && x < 13) ||
                     (k->pose == CKPOSE_SLEEP && y == 4 && x < 11) ||
                     (k->pose == CKPOSE_PLAY && y >= height - 2 && x >= 17) ||
                     (k->pose == CKPOSE_WALK && y == height - 1)) color = CKCLR_PAW;
            if (tail_phase && ((k->pose == CKPOSE_SIT && y == height - 1 && ch == '\'') ||
                               (k->pose == CKPOSE_SLEEP && y == height - 1 && ch == '\'') ||
                               ((k->pose == CKPOSE_PLAY || k->pose == CKPOSE_WALK) && y < 2 && ch == '`'))) {
                ch = ch == '\'' ? '`' : '\'';
            }
            sprite_put(k, c, width, top, x, y, ch, color);
        }
    }

    /* A pair of quiet tabby marks replaces the old noisy rectangular fill. */
    if (k->fur_density >= 58) {
        static const int marks[4][3] = {{5, 11, 5}, {13, 14, 4}, {7, 8, 3}, {5, 6, 3}};
        int pose = k->pose >= CKPOSE_SIT && k->pose <= CKPOSE_WALK ? (int)k->pose : CKPOSE_WALK;
        for (int i = 0; i < 2; i++) {
            int x = marks[pose][i];
            int y = marks[pose][2];
            int actual_x = k->cx - width / 2 + (k->facing < 0 ? width - 1 - x : x);
            if (ckitty_canvas_get(c, actual_x, top + y) == ' ') {
                sprite_put(k, c, width, top, x, y, k->fur_a == '.' ? '.' : '\'', CKCLR_FUR);
            }
        }
    }
}

static void render_bird(ckitty_canvas* c, int x, int y) {
    put_str(c, x, y, "v^v", CKCLR_ACCENT);
}

static void render_sitting(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    render_shape(k, frame, c, sit_shape, 8, k->cy - 3);
    if (k->has_bird) render_bird(c, k->cx + k->bird_dx, k->cy - 3 + k->bird_dy);
}

static void render_sleeping(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    render_shape(k, frame, c, sleep_shape, 7, k->cy - 3);
    if (k->has_bird) render_bird(c, k->cx + k->bird_dx, k->cy + k->bird_dy - 2);
}

static void render_playing(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    int phase = (int)((frame / 18U) % 4U);
    int bob = phase == 1 ? -1 : phase == 3 ? 1 : 0;
    render_shape(k, frame, c, play_shape, 7, k->cy - 2 + bob);
    int dir = k->facing < 0 ? -1 : 1;
    int toy_y = k->cy + 4;
    if (k->has_yarn) {
        int center = k->cx + dir * 18;
        put_str(c, center - 1, toy_y, "(@)", CKCLR_TOY);
        put(c, center - dir * 2, toy_y, '~', CKCLR_TOY);
        put(c, center - dir * 3, toy_y, '~', CKCLR_TOY);
    } else if (k->has_mouse) {
        put_str(c, k->cx + (dir > 0 ? 15 : -19), toy_y, "<:3~~", CKCLR_GRAY);
    }
    if (k->has_bird) render_bird(c, k->cx + k->bird_dx, k->cy - 6 + k->bird_dy);
}

static void render_walking(const ckitty_kitty* k, uint64_t frame, ckitty_canvas* c) {
    render_shape(k, frame, c, walk_shape, 7, k->cy - 2);
    if (k->has_bird) render_bird(c, k->cx + k->bird_dx, k->cy - 3 + k->bird_dy);
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
