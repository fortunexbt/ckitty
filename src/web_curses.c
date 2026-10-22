#define _XOPEN_SOURCE 700

/* ANSI-emitting backend for the ncurses subset declared in web_curses.h.
 *
 * ckitty.c draws with the same calls it makes natively; here they land in a
 * cell buffer that is flushed to the browser terminal as one ANSI frame, and
 * getch() yields to the event loop (Asyncify) instead of blocking. */

#include "web_curses.h"

#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

#define CKW_MIN_COLS 8
#define CKW_MIN_ROWS 4
#define CKW_MAX_COLS 400
#define CKW_MAX_ROWS 200
#define CKW_KEY_SLOTS 64
#define CKW_SLEEP_SLICE_MS 8

/* A wide glyph owns two cells; the trailing one is emitted as nothing because
 * the terminal advances the cursor past it on its own. */
#define CKW_CONTINUATION 0xfffffffeU

typedef struct {
    uint32_t cp;
    uint8_t pair;
    uint8_t bold;
} Cell;

struct ckitty_web_screen {
    int placeholder;
};

int LINES = 24;
int COLS = 80;
int COLORS = 256;
int COLOR_PAIRS = 256;
WINDOW* stdscr = NULL;

static struct ckitty_web_screen screen;
static Cell* cells = NULL;
static int grid_w = 0;
static int grid_h = 0;
static int cursor_y = 0;
static int cursor_x = 0;
static unsigned attr_pair = 0;
static int attr_bold = 0;
static int poll_ms = 50;
static short pair_fg[256];

static int want_cols = 80;
static int want_rows = 24;
static int resize_pending = 1;

static int keys[CKW_KEY_SLOTS];
static int keys_head = 0;
static int keys_tail = 0;

static char* out = NULL;
static size_t out_len = 0;
static size_t out_cap = 0;

EM_JS(void, ckw_write, (const char* bytes, int len), {
    ckittyTerminalWrite(HEAPU8.slice(bytes, bytes + len));
});

EM_JS(void, ckw_session_ended, (void), {
    ckittySessionEnded();
});

static int out_reserve(size_t extra) {
    if (out_len + extra <= out_cap) return 1;
    size_t cap = out_cap ? out_cap : 4096;
    while (cap < out_len + extra) cap *= 2;
    char* grown = realloc(out, cap);
    if (!grown) return 0;
    out = grown;
    out_cap = cap;
    return 1;
}

static void out_bytes(const char* text, size_t len) {
    if (!out_reserve(len)) return;
    memcpy(out + out_len, text, len);
    out_len += len;
}

static void out_str(const char* text) {
    out_bytes(text, strlen(text));
}

static void out_uint(unsigned value) {
    char digits[12];
    int n = 0;
    do {
        digits[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value && n < (int)sizeof(digits));
    while (n > 0) {
        char c = digits[--n];
        out_bytes(&c, 1);
    }
}

static void out_codepoint(uint32_t cp) {
    char buf[4];
    if (cp < 0x80U) {
        buf[0] = (char)cp;
        out_bytes(buf, 1);
    } else if (cp < 0x800U) {
        buf[0] = (char)(0xc0U | (cp >> 6));
        buf[1] = (char)(0x80U | (cp & 0x3fU));
        out_bytes(buf, 2);
    } else if (cp < 0x10000U) {
        buf[0] = (char)(0xe0U | (cp >> 12));
        buf[1] = (char)(0x80U | ((cp >> 6) & 0x3fU));
        buf[2] = (char)(0x80U | (cp & 0x3fU));
        out_bytes(buf, 3);
    } else {
        buf[0] = (char)(0xf0U | (cp >> 18));
        buf[1] = (char)(0x80U | ((cp >> 12) & 0x3fU));
        buf[2] = (char)(0x80U | ((cp >> 6) & 0x3fU));
        buf[3] = (char)(0x80U | (cp & 0x3fU));
        out_bytes(buf, 4);
    }
}

static void out_style(unsigned pair, int bold) {
    out_str("\x1b[0");
    if (bold) out_str(";1");
    short fg = pair < 256U ? pair_fg[pair] : -1;
    if (fg >= 0) {
        out_str(";38;5;");
        out_uint((unsigned)fg);
    } else {
        out_str(";39");
    }
    out_str("m");
}

static void out_flush(void) {
    if (out_len > 0) ckw_write(out, (int)out_len);
    out_len = 0;
}

static Cell* cell_at(int y, int x) {
    if (!cells || y < 0 || x < 0 || y >= grid_h || x >= grid_w) return NULL;
    return &cells[(size_t)y * (size_t)grid_w + (size_t)x];
}

static void grid_blank(void) {
    if (!cells) return;
    size_t count = (size_t)grid_w * (size_t)grid_h;
    for (size_t i = 0; i < count; i++) {
        cells[i].cp = ' ';
        cells[i].pair = 0;
        cells[i].bold = 0;
    }
}

static int clamp(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int grid_resize(int cols, int rows) {
    cols = clamp(cols, CKW_MIN_COLS, CKW_MAX_COLS);
    rows = clamp(rows, CKW_MIN_ROWS, CKW_MAX_ROWS);
    Cell* grown = realloc(cells, (size_t)cols * (size_t)rows * sizeof(Cell));
    if (!grown) return 0;
    cells = grown;
    grid_w = cols;
    grid_h = rows;
    COLS = cols;
    LINES = rows;
    cursor_y = 0;
    cursor_x = 0;
    grid_blank();
    return 1;
}

static void apply_resize(void) {
    if (!resize_pending) return;
    resize_pending = 0;
    if (want_cols == grid_w && want_rows == grid_h) return;
    (void)grid_resize(want_cols, want_rows);
}

/* Called from JS when the terminal element is measured or the window resizes;
 * the app's own loop then notices LINES/COLS moved and re-lays out. */
EMSCRIPTEN_KEEPALIVE void ckitty_web_resize(int cols, int rows) {
    want_cols = clamp(cols, CKW_MIN_COLS, CKW_MAX_COLS);
    want_rows = clamp(rows, CKW_MIN_ROWS, CKW_MAX_ROWS);
    resize_pending = 1;
}

EMSCRIPTEN_KEEPALIVE void ckitty_web_key(int key) {
    int next = (keys_tail + 1) % CKW_KEY_SLOTS;
    if (next == keys_head) return;  /* Drop keys only when the app is wedged. */
    keys[keys_tail] = key;
    keys_tail = next;
}

static int key_take(void) {
    if (keys_head == keys_tail) return ERR;
    int key = keys[keys_head];
    keys_head = (keys_head + 1) % CKW_KEY_SLOTS;
    return key;
}

WINDOW* initscr(void) {
    for (int i = 0; i < 256; i++) pair_fg[i] = -1;
    resize_pending = 1;
    if (!grid_resize(want_cols, want_rows)) return NULL;
    stdscr = &screen;
    out_str("\x1b[2J\x1b[H");
    out_flush();
    return stdscr;
}

int endwin(void) {
    out_str("\x1b[0m\x1b[?25h");
    out_flush();
    ckw_session_ended();
    return OK;
}

int cbreak(void) { return OK; }
int noecho(void) { return OK; }

int curs_set(int visibility) {
    out_str(visibility == 0 ? "\x1b[?25l" : "\x1b[?25h");
    out_flush();
    return OK;
}

void timeout(int delay_ms) {
    poll_ms = clamp(delay_ms, 0, 1000);
}

void set_escdelay(int delay_ms) { (void)delay_ms; }

int has_colors(void) { return TRUE; }
int start_color(void) { return OK; }
int use_default_colors(void) { return OK; }

int init_pair(short pair, short foreground, short background) {
    (void)background;  /* The page owns the background, like a real terminal. */
    if (pair < 0 || pair >= 256) return ERR;
    pair_fg[pair] = foreground;
    return OK;
}

int attrset(chtype attrs) {
    attr_pair = (unsigned)(attrs & 0xffUL);
    attr_bold = (attrs & A_BOLD) != 0;
    return OK;
}

int attron(chtype attrs) {
    unsigned pair = (unsigned)(attrs & 0xffUL);
    if (pair) attr_pair = pair;
    if (attrs & A_BOLD) attr_bold = 1;
    return OK;
}

int erase(void) {
    grid_blank();
    cursor_y = 0;
    cursor_x = 0;
    return OK;
}

int move(int y, int x) {
    if (y < 0 || x < 0 || y >= grid_h || x >= grid_w) return ERR;
    cursor_y = y;
    cursor_x = x;
    return OK;
}

static void put_cell(int y, int x, uint32_t cp) {
    Cell* cell = cell_at(y, x);
    if (!cell) return;
    cell->cp = cp;
    cell->pair = (uint8_t)attr_pair;
    cell->bold = (uint8_t)attr_bold;
}

int addnwstr(const wchar_t* text, int n) {
    if (!text) return ERR;
    for (int i = 0; i < n && text[i]; i++) {
        uint32_t cp = (uint32_t)text[i];
        int width = wcwidth(text[i]);
        if (width < 0) {
            cp = ' ';
            width = 1;
        }
        if (width == 0) continue;
        if (cursor_x + width > grid_w) return ERR;
        put_cell(cursor_y, cursor_x, cp);
        for (int pad = 1; pad < width; pad++) {
            put_cell(cursor_y, cursor_x + pad, CKW_CONTINUATION);
        }
        cursor_x += width;
    }
    return OK;
}

int mvaddch(int y, int x, chtype ch) {
    if (y < 0 || x < 0 || y >= grid_h || x >= grid_w) return ERR;
    put_cell(y, x, (uint32_t)(ch & 0xffUL));
    cursor_y = y;
    cursor_x = x + 1 < grid_w ? x + 1 : x;
    return OK;
}

int mvhline(int y, int x, chtype ch, int n) {
    if (y < 0 || x < 0 || y >= grid_h || x >= grid_w) return ERR;
    for (int i = 0; i < n && x + i < grid_w; i++) {
        put_cell(y, x + i, (uint32_t)(ch & 0xffUL));
    }
    cursor_y = y;
    cursor_x = x;
    return OK;
}

int refresh(void) {
    if (!cells) return ERR;
    out_str("\x1b[H");
    unsigned pair = 0;
    int bold = 0;
    int styled = 0;
    for (int y = 0; y < grid_h; y++) {
        if (y > 0) out_str("\r\n");
        int last = -1;
        for (int x = grid_w - 1; x >= 0; x--) {
            const Cell* cell = cell_at(y, x);
            if (cell && cell->cp != ' ') {
                last = x;
                break;
            }
        }
        for (int x = 0; x <= last; x++) {
            const Cell* cell = cell_at(y, x);
            if (!cell || cell->cp == CKW_CONTINUATION) continue;
            if (!styled || cell->pair != pair || cell->bold != bold) {
                out_style(cell->pair, cell->bold);
                pair = cell->pair;
                bold = cell->bold;
                styled = 1;
            }
            out_codepoint(cell->cp);
        }
        out_str("\x1b[K");
    }
    out_str("\x1b[0m");
    out_flush();
    return OK;
}

int getch(void) {
    apply_resize();
    int key = key_take();
    if (key != ERR) return key;
    /* Always yield at least once: a zero timeout must not spin the main thread. */
    int budget = poll_ms > 0 ? poll_ms : 1;
    for (int waited = 0; waited < budget; waited += CKW_SLEEP_SLICE_MS) {
        int slice = budget - waited;
        if (slice > CKW_SLEEP_SLICE_MS) slice = CKW_SLEEP_SLICE_MS;
        emscripten_sleep((unsigned)slice);
        key = key_take();
        if (key != ERR) return key;
        /* A resize must reach the layout as promptly as a keypress does. */
        if (resize_pending) {
            apply_resize();
            return ERR;
        }
    }
    return ERR;
}
