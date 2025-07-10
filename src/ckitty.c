#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#define DELAY_DEFAULT 50000
#define MAX_FRAMES 8

typedef struct {
    int x;
    int y;
    int direction;
    int frame;
    int state;
    int color_pair;
} Kitty;

typedef struct {
    int delay;
    int colors;
    int infinite;
    int rainbow;
} Config;

const char* kitty_idle[3][5] = {
    {
        "  /\\_/\\  ",
        " ( o.o ) ",
        "  > ^ <  ",
        " /     \\ ",
        "(_)   (_)"
    },
    {
        "  /\\_/\\  ",
        " ( -.o ) ",
        "  > ^ <  ",
        " /     \\ ",
        "(_)   (_)"
    },
    {
        "  /\\_/\\  ",
        " ( o.- ) ",
        "  > ^ <  ",
        " /     \\ ",
        "(_)   (_)"
    }
};

const char* kitty_walk_right[2][5] = {
    {
        "  /\\_/\\  ",
        " ( o.o ) ",
        "  > ^ <  ",
        " /  |  \\ ",
        "(o)   (o)"
    },
    {
        "  /\\_/\\  ",
        " ( o.o ) ",
        "  > ^ <  ",
        " /  |  \\ ",
        "  (o)(o) "
    }
};

const char* kitty_walk_left[2][5] = {
    {
        "  /\\_/\\  ",
        " ( o.o ) ",
        "  > ^ <  ",
        " /  |  \\ ",
        "(o)   (o)"
    },
    {
        "  /\\_/\\  ",
        " ( o.o ) ",
        "  > ^ <  ",
        " /  |  \\ ",
        " (o)(o)  "
    }
};

const char* kitty_sleep[2][5] = {
    {
        "  /\\_/\\  ",
        " ( -.-)  ",
        "  > ^ <  ",
        " /     \\ ",
        "(_)   (_)"
    },
    {
        "  /\\_/\\  ",
        " ( -.-) z",
        "  > ^ <  ",
        " /     \\ ",
        "(_)   (_)"
    }
};

void init_colors(void) {
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_RED, COLOR_BLACK);
    init_pair(7, COLOR_BLUE, COLOR_BLACK);
}

void draw_kitty(Kitty* kitty, const char* frame[5]) {
    attron(COLOR_PAIR(kitty->color_pair));
    for (int i = 0; i < 5; i++) {
        mvprintw(kitty->y + i, kitty->x, "%s", frame[i]);
    }
    attroff(COLOR_PAIR(kitty->color_pair));
}

void update_kitty(Kitty* kitty, int max_x, int max_y, Config* config) {
    static int frame_counter = 0;
    frame_counter++;
    
    if (frame_counter % 10 == 0) {
        kitty->frame = (kitty->frame + 1) % 2;
        
        if (config->rainbow) {
            kitty->color_pair = (rand() % 7) + 1;
        }
    }
    
    if (rand() % 100 < 5) {
        kitty->state = rand() % 4;
    }
    
    switch (kitty->state) {
        case 1:
            if (kitty->x < max_x - 12) {
                kitty->x++;
                kitty->direction = 1;
            }
            break;
        case 2:
            if (kitty->x > 1) {
                kitty->x--;
                kitty->direction = -1;
            }
            break;
        case 3:
            break;
        default:
            break;
    }
}

void print_usage(void) {
    printf("ckitty - Terminal kitty generator\n\n");
    printf("Usage: ckitty [OPTIONS]\n\n");
    printf("OPTIONS:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -d, --delay <ms>    Set animation delay in microseconds (default: 50000)\n");
    printf("  -c, --colors        Enable colors\n");
    printf("  -r, --rainbow       Enable rainbow mode\n");
    printf("  -i, --infinite      Run indefinitely\n");
}

int main(int argc, char *argv[]) {
    Config config = {
        .delay = DELAY_DEFAULT,
        .colors = 0,
        .infinite = 0,
        .rainbow = 0
    };
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"delay", required_argument, 0, 'd'},
        {"colors", no_argument, 0, 'c'},
        {"rainbow", no_argument, 0, 'r'},
        {"infinite", no_argument, 0, 'i'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hd:cri", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'd':
                config.delay = atoi(optarg);
                break;
            case 'c':
                config.colors = 1;
                break;
            case 'r':
                config.rainbow = 1;
                config.colors = 1;
                break;
            case 'i':
                config.infinite = 1;
                break;
            default:
                print_usage();
                return 1;
        }
    }
    
    srand(time(NULL));
    
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    
    if (has_colors() && config.colors) {
        init_colors();
    }
    
    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);
    
    Kitty kitty = {
        .x = max_x / 2 - 5,
        .y = max_y / 2 - 3,
        .direction = 0,
        .frame = 0,
        .state = 0,
        .color_pair = config.colors ? (rand() % 7) + 1 : 0
    };
    
    int ch;
    int running = 1;
    int cycles = 0;
    
    while (running) {
        clear();
        
        update_kitty(&kitty, max_x, max_y, &config);
        
        const char** current_frame = NULL;
        if (kitty.state == 0) {
            current_frame = kitty_idle[kitty.frame % 3];
        } else if (kitty.state == 1) {
            current_frame = kitty_walk_right[kitty.frame % 2];
        } else if (kitty.state == 2) {
            current_frame = kitty_walk_left[kitty.frame % 2];
        } else if (kitty.state == 3) {
            current_frame = kitty_sleep[kitty.frame % 2];
        }
        
        if (current_frame) {
            draw_kitty(&kitty, current_frame);
        }
        
        refresh();
        
        ch = getch();
        if (ch == 'q' || ch == 27) {
            running = 0;
        }
        
        if (!config.infinite) {
            cycles++;
            if (cycles > 1000) {
                running = 0;
            }
        }
        
        usleep(config.delay);
    }
    
    endwin();
    return 0;
}
