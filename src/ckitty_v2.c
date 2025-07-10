#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

#define DELAY_DEFAULT 50000
#define GROW_DELAY 80000
#define MAX_TAIL_LENGTH 20
#define MAX_WHISKERS 6

typedef enum {
    PART_NONE,
    PART_TAIL_BASE,
    PART_TAIL,
    PART_BODY,
    PART_NECK,
    PART_HEAD,
    PART_EAR_LEFT,
    PART_EAR_RIGHT,
    PART_WHISKERS,
    PART_LEGS,
    PART_EYES,
    PART_NOSE,
    PART_MOUTH,
    PART_FUR
} KittyPart;

typedef enum {
    TAIL_STRAIGHT,
    TAIL_CURVED_UP,
    TAIL_CURVED_DOWN,
    TAIL_QUESTION,
    TAIL_EXCITED
} TailType;

typedef struct {
    int x;
    int y;
    char ch;
    int color;
    KittyPart part;
    int age;
} KittyPixel;

typedef struct {
    KittyPixel* pixels;
    int pixel_count;
    int pixel_capacity;
    int growth_stage;
    int tail_type;
    int body_width;
    int body_height;
    int fluffiness;
    int whisker_length;
    int ear_type;
    int tail_x;
    int tail_y;
    int head_x;
    int head_y;
    int base_x;
    int base_y;
} Kitty;

typedef struct {
    int delay;
    int colors;
    int rainbow;
    int live;
    int infinite;
    int seed;
    int base_type;
    int verbosity;
    int screensaver;
} Config;

const char tail_segments[] = {'/', '\\', '|', '_', '~', ')', '(', 'S'};
const char body_fill[] = {'#', '@', '*', 'o', 'O', '.', ':', '+'};
const char fur_texture[] = {'~', '`', '\'', '^', ',', '.'};

void init_colors(void) {
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_BLACK, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_RED, COLOR_BLACK);
    init_pair(7, COLOR_GREEN, COLOR_BLACK);
    init_pair(8, COLOR_BLUE, COLOR_BLACK);
    
    init_pair(10, COLOR_BLACK, COLOR_WHITE);
    init_pair(11, COLOR_WHITE, COLOR_YELLOW);
    init_pair(12, COLOR_BLACK, COLOR_CYAN);
}

void add_pixel(Kitty* kitty, int x, int y, char ch, int color, KittyPart part) {
    if (kitty->pixel_count >= kitty->pixel_capacity) {
        kitty->pixel_capacity *= 2;
        kitty->pixels = realloc(kitty->pixels, sizeof(KittyPixel) * kitty->pixel_capacity);
    }
    
    kitty->pixels[kitty->pixel_count].x = x;
    kitty->pixels[kitty->pixel_count].y = y;
    kitty->pixels[kitty->pixel_count].ch = ch;
    kitty->pixels[kitty->pixel_count].color = color;
    kitty->pixels[kitty->pixel_count].part = part;
    kitty->pixels[kitty->pixel_count].age = 0;
    kitty->pixel_count++;
}

void grow_tail(Kitty* kitty, int life, int x, int y, int direction) {
    if (life <= 0) return;
    
    int dx = 0, dy = 0;
    char segment = '~';
    
    switch (kitty->tail_type) {
        case TAIL_CURVED_UP:
            dx = (rand() % 3) - 1;
            dy = (life > MAX_TAIL_LENGTH/2) ? -1 : 0;
            segment = (dx > 0) ? '\\' : (dx < 0) ? '/' : '|';
            break;
        case TAIL_CURVED_DOWN:
            dx = (rand() % 3) - 1;
            dy = (life > MAX_TAIL_LENGTH/2) ? 0 : 1;
            segment = (dx > 0) ? '/' : (dx < 0) ? '\\' : '|';
            break;
        case TAIL_QUESTION:
            if (life > MAX_TAIL_LENGTH * 0.7) {
                dy = -1;
                segment = '|';
            } else if (life > MAX_TAIL_LENGTH * 0.3) {
                dx = 1;
                segment = '_';
            } else {
                dy = 1;
                dx = 1;
                segment = '\\';
            }
            break;
        case TAIL_EXCITED:
            dx = (rand() % 3) - 1;
            dy = (rand() % 2) - 1;
            segment = tail_segments[rand() % 8];
            break;
        default:
            dx = direction;
            segment = (direction > 0) ? '~' : '~';
            break;
    }
    
    add_pixel(kitty, x, y, segment, rand() % 7 + 1, PART_TAIL);
    
    if (rand() % 100 < 80) {
        grow_tail(kitty, life - 1, x + dx, y + dy, dx);
    }
}

void grow_body(Kitty* kitty) {
    int start_x = kitty->base_x;
    int start_y = kitty->base_y;
    
    for (int i = 0; i < kitty->body_height; i++) {
        for (int j = 0; j < kitty->body_width; j++) {
            char ch = ' ';
            int color = rand() % 7 + 1;
            
            if (i == 0 || i == kitty->body_height - 1) {
                if (j == 0) ch = '(';
                else if (j == kitty->body_width - 1) ch = ')';
                else ch = '_';
            } else if (j == 0 || j == kitty->body_width - 1) {
                ch = '|';
            } else {
                if (rand() % 100 < kitty->fluffiness) {
                    ch = body_fill[rand() % 8];
                } else {
                    ch = ' ';
                }
            }
            
            if (ch != ' ') {
                add_pixel(kitty, start_x + j, start_y - i, ch, color, PART_BODY);
            }
        }
    }
    
    kitty->head_x = start_x + kitty->body_width / 2;
    kitty->head_y = start_y - kitty->body_height;
}

void grow_head(Kitty* kitty) {
    int cx = kitty->head_x;
    int cy = kitty->head_y;
    
    add_pixel(kitty, cx - 2, cy - 1, '/', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx - 1, cy - 1, '_', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx, cy - 1, '_', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx + 1, cy - 1, '_', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx + 2, cy - 1, '\\', rand() % 7 + 1, PART_HEAD);
    
    add_pixel(kitty, cx - 3, cy, '(', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx + 3, cy, ')', rand() % 7 + 1, PART_HEAD);
    
    add_pixel(kitty, cx - 2, cy + 1, '\\', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx - 1, cy + 1, '_', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx, cy + 1, '_', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx + 1, cy + 1, '_', rand() % 7 + 1, PART_HEAD);
    add_pixel(kitty, cx + 2, cy + 1, '/', rand() % 7 + 1, PART_HEAD);
}

void grow_ears(Kitty* kitty) {
    int cx = kitty->head_x;
    int cy = kitty->head_y;
    
    if (kitty->ear_type == 0) {
        add_pixel(kitty, cx - 3, cy - 2, '/', rand() % 7 + 1, PART_EAR_LEFT);
        add_pixel(kitty, cx - 2, cy - 2, '\\', rand() % 7 + 1, PART_EAR_LEFT);
        add_pixel(kitty, cx + 2, cy - 2, '/', rand() % 7 + 1, PART_EAR_RIGHT);
        add_pixel(kitty, cx + 3, cy - 2, '\\', rand() % 7 + 1, PART_EAR_RIGHT);
    } else {
        add_pixel(kitty, cx - 3, cy - 2, '^', rand() % 7 + 1, PART_EAR_LEFT);
        add_pixel(kitty, cx + 3, cy - 2, '^', rand() % 7 + 1, PART_EAR_RIGHT);
    }
}

void grow_face(Kitty* kitty) {
    int cx = kitty->head_x;
    int cy = kitty->head_y;
    
    if (rand() % 100 < 50) {
        add_pixel(kitty, cx - 1, cy, 'o', COLOR_PAIR(7), PART_EYES);
        add_pixel(kitty, cx + 1, cy, 'o', COLOR_PAIR(7), PART_EYES);
    } else {
        add_pixel(kitty, cx - 1, cy, '-', COLOR_PAIR(1), PART_EYES);
        add_pixel(kitty, cx + 1, cy, '-', COLOR_PAIR(1), PART_EYES);
    }
    
    add_pixel(kitty, cx, cy, '^', COLOR_PAIR(6), PART_NOSE);
    
    if (rand() % 100 < 30) {
        add_pixel(kitty, cx, cy + 1, 'w', COLOR_PAIR(6), PART_MOUTH);
    }
}

void grow_whiskers(Kitty* kitty) {
    int cx = kitty->head_x;
    int cy = kitty->head_y;
    
    for (int i = 0; i < 3; i++) {
        int len = rand() % kitty->whisker_length + 2;
        for (int j = 1; j <= len; j++) {
            add_pixel(kitty, cx - 3 - j, cy - 1 + i, '-', COLOR_PAIR(1), PART_WHISKERS);
            add_pixel(kitty, cx + 3 + j, cy - 1 + i, '-', COLOR_PAIR(1), PART_WHISKERS);
        }
    }
}

void grow_legs(Kitty* kitty) {
    int bx = kitty->base_x;
    int by = kitty->base_y;
    
    add_pixel(kitty, bx + 1, by + 1, '|', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + 1, by + 2, '|', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + kitty->body_width - 2, by + 1, '|', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + kitty->body_width - 2, by + 2, '|', rand() % 7 + 1, PART_LEGS);
    
    add_pixel(kitty, bx, by + 3, '(', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + 1, by + 3, '_', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + 2, by + 3, ')', rand() % 7 + 1, PART_LEGS);
    
    add_pixel(kitty, bx + kitty->body_width - 3, by + 3, '(', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + kitty->body_width - 2, by + 3, '_', rand() % 7 + 1, PART_LEGS);
    add_pixel(kitty, bx + kitty->body_width - 1, by + 3, ')', rand() % 7 + 1, PART_LEGS);
}

void add_fur_details(Kitty* kitty) {
    for (int i = 0; i < kitty->pixel_count; i++) {
        if (kitty->pixels[i].part == PART_BODY && kitty->pixels[i].ch == ' ') {
            if (rand() % 100 < 5) {
                kitty->pixels[i].ch = fur_texture[rand() % 6];
                kitty->pixels[i].part = PART_FUR;
            }
        }
    }
}

void grow_kitty(Kitty* kitty, Config* config) {
    switch (kitty->growth_stage) {
        case 0:
            kitty->tail_x = kitty->base_x - 2;
            kitty->tail_y = kitty->base_y - 2;
            grow_tail(kitty, MAX_TAIL_LENGTH, kitty->tail_x, kitty->tail_y, -1);
            break;
        case 1:
            grow_body(kitty);
            break;
        case 2:
            grow_legs(kitty);
            break;
        case 3:
            grow_head(kitty);
            break;
        case 4:
            grow_ears(kitty);
            break;
        case 5:
            grow_face(kitty);
            break;
        case 6:
            grow_whiskers(kitty);
            break;
        case 7:
            add_fur_details(kitty);
            break;
    }
    
    kitty->growth_stage++;
}

void draw_kitty(Kitty* kitty, Config* config) {
    for (int i = 0; i < kitty->pixel_count; i++) {
        KittyPixel* p = &kitty->pixels[i];
        
        if (config->colors) {
            attron(COLOR_PAIR(p->color));
        }
        
        mvaddch(p->y, p->x, p->ch);
        
        if (config->colors) {
            attroff(COLOR_PAIR(p->color));
        }
        
        p->age++;
    }
}

void animate_kitty(Kitty* kitty) {
    static int frame = 0;
    frame++;
    
    for (int i = 0; i < kitty->pixel_count; i++) {
        KittyPixel* p = &kitty->pixels[i];
        
        if (p->part == PART_TAIL && frame % 20 == 0) {
            if (rand() % 100 < 20) {
                p->y += (rand() % 3) - 1;
            }
        }
        
        if (p->part == PART_EYES && frame % 60 == 0) {
            if (p->ch == 'o') p->ch = '-';
            else if (p->ch == '-') p->ch = 'o';
        }
        
        if (p->part == PART_WHISKERS && frame % 40 == 0) {
            p->x += (rand() % 3) - 1;
        }
    }
}

void print_usage(void) {
    printf("ckitty v2 - Procedural kitty generator\n\n");
    printf("Usage: ckitty [OPTIONS]\n\n");
    printf("OPTIONS:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -l, --live          Show live generation\n");
    printf("  -d, --delay <ms>    Animation delay (default: 50000)\n");
    printf("  -c, --colors        Enable colors\n");
    printf("  -r, --rainbow       Rainbow mode\n");
    printf("  -s, --seed <num>    Set random seed\n");
    printf("  -i, --infinite      Run indefinitely\n");
    printf("  -S, --screensaver   Screensaver mode\n");
}

int main(int argc, char *argv[]) {
    Config config = {
        .delay = DELAY_DEFAULT,
        .colors = 0,
        .rainbow = 0,
        .live = 0,
        .infinite = 0,
        .seed = time(NULL),
        .screensaver = 0
    };
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"live", no_argument, 0, 'l'},
        {"delay", required_argument, 0, 'd'},
        {"colors", no_argument, 0, 'c'},
        {"rainbow", no_argument, 0, 'r'},
        {"seed", required_argument, 0, 's'},
        {"infinite", no_argument, 0, 'i'},
        {"screensaver", no_argument, 0, 'S'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hld:crs:iS", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'l':
                config.live = 1;
                break;
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
            case 's':
                config.seed = atoi(optarg);
                break;
            case 'i':
                config.infinite = 1;
                break;
            case 'S':
                config.screensaver = 1;
                config.infinite = 1;
                break;
            default:
                print_usage();
                return 1;
        }
    }
    
    srand(config.seed);
    
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
    
    Kitty kitty = {0};
    kitty.pixels = malloc(sizeof(KittyPixel) * 1000);
    kitty.pixel_capacity = 1000;
    kitty.pixel_count = 0;
    kitty.growth_stage = 0;
    kitty.tail_type = rand() % 5;
    kitty.body_width = 8 + rand() % 4;
    kitty.body_height = 4 + rand() % 2;
    kitty.fluffiness = 20 + rand() % 40;
    kitty.whisker_length = 3 + rand() % 3;
    kitty.ear_type = rand() % 2;
    kitty.base_x = max_x / 2 - kitty.body_width / 2;
    kitty.base_y = max_y / 2;
    
    int ch;
    int running = 1;
    int complete = 0;
    
    while (running) {
        clear();
        
        if (!complete && kitty.growth_stage < 8) {
            grow_kitty(&kitty, &config);
            
            if (config.live) {
                draw_kitty(&kitty, &config);
                refresh();
                usleep(GROW_DELAY);
            }
        } else {
            complete = 1;
            animate_kitty(&kitty);
        }
        
        if (!config.live || complete) {
            draw_kitty(&kitty, &config);
            refresh();
            usleep(config.delay);
        }
        
        ch = getch();
        if (ch == 'q' || ch == 27) {
            running = 0;
        }
        
        if (!config.infinite && complete) {
            static int wait_cycles = 0;
            wait_cycles++;
            if (wait_cycles > 200) {
                running = 0;
            }
        }
        
        if (config.screensaver && complete) {
            static int screensaver_timer = 0;
            screensaver_timer++;
            if (screensaver_timer > 500) {
                clear();
                free(kitty.pixels);
                
                kitty.pixels = malloc(sizeof(KittyPixel) * 1000);
                kitty.pixel_capacity = 1000;
                kitty.pixel_count = 0;
                kitty.growth_stage = 0;
                kitty.tail_type = rand() % 5;
                kitty.body_width = 8 + rand() % 4;
                kitty.body_height = 4 + rand() % 2;
                kitty.fluffiness = 20 + rand() % 40;
                kitty.whisker_length = 3 + rand() % 3;
                kitty.ear_type = rand() % 2;
                kitty.base_x = rand() % (max_x - 20) + 10;
                kitty.base_y = rand() % (max_y - 10) + 5;
                
                complete = 0;
                screensaver_timer = 0;
            }
        }
    }
    
    free(kitty.pixels);
    endwin();
    return 0;
}
