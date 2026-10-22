#ifndef CKITTY_WEB_CURSES_H
#define CKITTY_WEB_CURSES_H

#include <stdint.h>
#include <wchar.h>

/* The ncurses subset ckitty.c actually uses, backed by a cell buffer that is
 * flushed to a browser terminal as ANSI. Only the WebAssembly build compiles
 * this; native builds keep linking real ncurses. */

#define NCURSES_VERSION "ckitty-web"

#define ERR (-1)
#define OK 0
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef unsigned long chtype;
typedef struct ckitty_web_screen WINDOW;

#define A_NORMAL ((chtype)0)
#define A_BOLD ((chtype)0x10000UL)

#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7

#define COLOR_PAIR(n) ((chtype)((unsigned)(n) & 0xffU))

extern int LINES;
extern int COLS;
extern int COLORS;
extern int COLOR_PAIRS;
extern WINDOW* stdscr;

#define keypad(win, flag) ((void)(win), (void)(flag), OK)
#define getmaxyx(win, rows, cols) \
    do { (void)(win); (rows) = LINES; (cols) = COLS; } while (0)

WINDOW* initscr(void);
int endwin(void);
int cbreak(void);
int noecho(void);
int curs_set(int visibility);
void timeout(int delay_ms);
void set_escdelay(int delay_ms);

int has_colors(void);
int start_color(void);
int use_default_colors(void);
int init_pair(short pair, short foreground, short background);

int attrset(chtype attrs);
int attron(chtype attrs);

int erase(void);
int move(int y, int x);
int addnwstr(const wchar_t* text, int n);
int mvaddch(int y, int x, chtype ch);
int mvhline(int y, int x, chtype ch, int n);
int refresh(void);
int getch(void);

#endif  /* CKITTY_WEB_CURSES_H */
