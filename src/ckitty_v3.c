#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

#define DELAY_DEFAULT 40000
#define GROW_DELAY 60000
#define MAX_PIXELS 5000

typedef enum {
    KITTY_SITTING,
    KITTY_SLEEPING,
    KITTY_PLAYING,
    KITTY_WALKING
} KittyPose;

typedef struct {
    int x;
    int y;
    char ch;
    int color;
    int age;
    int visible;
} Pixel;

typedef struct {
    Pixel pixels[MAX_PIXELS];
    int pixel_count;
    int cx, cy;  // center position
    KittyPose pose;
    int direction;  // -1 left, 1 right
    int animation_frame;
    double tail_curl;
    int whisker_twitch;
    int ear_perk;
    int eye_state;  // 0: open, 1: blink, 2: sleep
} Kitty;

typedef struct {
    int delay;
    int grow_delay;
    int live;
    int colors;
    int screensaver;
    int infinite;
    int rainbow;
    int seed;
    char* message;
} Config;

// Color pairs
#define PAIR_WHITE 1
#define PAIR_BLACK 2
#define PAIR_ORANGE 3
#define PAIR_PINK 4
#define PAIR_YELLOW 5
#define PAIR_CYAN 6
#define PAIR_GREEN 7
#define PAIR_GRAY 8

void init_colors(void) {
    start_color();
    init_pair(PAIR_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_BLACK, COLOR_BLACK, COLOR_BLACK);
    init_pair(PAIR_ORANGE, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_PINK, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_GRAY, COLOR_WHITE, COLOR_BLACK);
}

void add_pixel(Kitty* kitty, int x, int y, char ch, int color) {
    if (kitty->pixel_count >= MAX_PIXELS) return;
    
    Pixel* p = &kitty->pixels[kitty->pixel_count];
    p->x = x;
    p->y = y;
    p->ch = ch;
    p->color = color;
    p->age = 0;
    p->visible = 1;
    kitty->pixel_count++;
}

void draw_curved_line(Kitty* kitty, int x0, int y0, int x1, int y1, double curve, char ch, int color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int steps = (dx > dy) ? dx : dy;
    if (steps == 0) steps = 1;
    
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        double x = x0 + t * (x1 - x0);
        double y = y0 + t * (y1 - y0);
        
        // Add curve
        double curve_offset = curve * sin(t * M_PI);
        if (dx > dy) {
            y += curve_offset;
        } else {
            x += curve_offset;
        }
        
        add_pixel(kitty, (int)x, (int)y, ch, color);
    }
}

void generate_tail(Kitty* kitty, int base_x, int base_y, int length, double curl_factor) {
    double angle = (kitty->direction > 0) ? M_PI : 0;
    double curve_amount = curl_factor * sin(kitty->animation_frame * 0.1);
    
    for (int i = 0; i < length; i++) {
        angle += (0.1 + curve_amount * 0.05) * kitty->direction;
        
        int x = base_x + (int)(cos(angle) * i * 0.8);
        int y = base_y - (int)(sin(angle) * i * 0.3);
        
        char ch = '~';
        if (i < length * 0.3) ch = 's';
        else if (i < length * 0.6) ch = '~';
        else ch = '.';
        
        add_pixel(kitty, x, y, ch, PAIR_ORANGE);
    }
}

void generate_body_sitting(Kitty* kitty) {
    int cx = kitty->cx;
    int cy = kitty->cy;
    
    // Body outline
    draw_curved_line(kitty, cx - 3, cy + 2, cx - 2, cy - 2, 0.5, '(', PAIR_ORANGE);
    draw_curved_line(kitty, cx + 3, cy + 2, cx + 2, cy - 2, -0.5, ')', PAIR_ORANGE);
    
    // Body fill
    for (int y = -1; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            if (rand() % 100 < 70) {
                add_pixel(kitty, cx + x, cy + y, '*', PAIR_ORANGE);
            }
        }
    }
    
    // Front paws
    add_pixel(kitty, cx - 1, cy + 3, '(', PAIR_WHITE);
    add_pixel(kitty, cx, cy + 3, '_', PAIR_WHITE);
    add_pixel(kitty, cx + 1, cy + 3, ')', PAIR_WHITE);
}

void generate_head(Kitty* kitty) {
    int cx = kitty->cx;
    int cy = kitty->cy - 3;
    
    // Head shape
    add_pixel(kitty, cx - 2, cy - 1, '/', PAIR_ORANGE);
    add_pixel(kitty, cx - 1, cy - 1, '_', PAIR_ORANGE);
    add_pixel(kitty, cx, cy - 1, '_', PAIR_ORANGE);
    add_pixel(kitty, cx + 1, cy - 1, '_', PAIR_ORANGE);
    add_pixel(kitty, cx + 2, cy - 1, '\\', PAIR_ORANGE);
    
    add_pixel(kitty, cx - 3, cy, '(', PAIR_ORANGE);
    add_pixel(kitty, cx + 3, cy, ')', PAIR_ORANGE);
    
    add_pixel(kitty, cx - 2, cy + 1, '\\', PAIR_ORANGE);
    add_pixel(kitty, cx + 2, cy + 1, '/', PAIR_ORANGE);
    
    // Ears
    if (kitty->ear_perk > 0) {
        add_pixel(kitty, cx - 2, cy - 2, '/', PAIR_ORANGE);
        add_pixel(kitty, cx - 1, cy - 2, '\\', PAIR_ORANGE);
        add_pixel(kitty, cx + 1, cy - 2, '/', PAIR_ORANGE);
        add_pixel(kitty, cx + 2, cy - 2, '\\', PAIR_ORANGE);
    } else {
        add_pixel(kitty, cx - 3, cy - 1, '_', PAIR_ORANGE);
        add_pixel(kitty, cx + 3, cy - 1, '_', PAIR_ORANGE);
    }
    
    // Face
    if (kitty->eye_state == 0) {
        add_pixel(kitty, cx - 1, cy, 'o', PAIR_BLACK);
        add_pixel(kitty, cx + 1, cy, 'o', PAIR_BLACK);
    } else if (kitty->eye_state == 1) {
        add_pixel(kitty, cx - 1, cy, '-', PAIR_BLACK);
        add_pixel(kitty, cx + 1, cy, '-', PAIR_BLACK);
    } else {
        add_pixel(kitty, cx - 1, cy, '^', PAIR_BLACK);
        add_pixel(kitty, cx + 1, cy, '^', PAIR_BLACK);
    }
    
    // Nose and mouth
    add_pixel(kitty, cx, cy, '^', PAIR_PINK);
    add_pixel(kitty, cx, cy + 1, 'w', PAIR_PINK);
    
    // Whiskers
    int whisker_offset = (kitty->whisker_twitch > 0) ? 1 : 0;
    for (int i = 1; i <= 3; i++) {
        add_pixel(kitty, cx - 3 - i, cy - 1 + whisker_offset, '-', PAIR_GRAY);
        add_pixel(kitty, cx + 3 + i, cy - 1 + whisker_offset, '-', PAIR_GRAY);
    }
    for (int i = 1; i <= 2; i++) {
        add_pixel(kitty, cx - 3 - i, cy + whisker_offset, '-', PAIR_GRAY);
        add_pixel(kitty, cx + 3 + i, cy + whisker_offset, '-', PAIR_GRAY);
    }
}

void generate_sleeping_kitty(Kitty* kitty) {
    int cx = kitty->cx;
    int cy = kitty->cy;
    
    // Curled up body
    for (int angle = 0; angle < 360; angle += 10) {
        double rad = angle * M_PI / 180.0;
        int r = 4 + (rand() % 2);
        int x = cx + (int)(cos(rad) * r);
        int y = cy + (int)(sin(rad) * r * 0.6);
        
        char ch = (angle < 90 || angle > 270) ? '(' : ')';
        if (angle > 45 && angle < 135) ch = '_';
        if (angle > 225 && angle < 315) ch = '_';
        
        add_pixel(kitty, x, y, ch, PAIR_ORANGE);
    }
    
    // Sleeping face
    add_pixel(kitty, cx - 1, cy - 1, '^', PAIR_BLACK);
    add_pixel(kitty, cx + 1, cy - 1, '^', PAIR_BLACK);
    add_pixel(kitty, cx, cy, 'w', PAIR_PINK);
    
    // Z's for sleeping
    if (kitty->animation_frame % 30 < 15) {
        add_pixel(kitty, cx + 5, cy - 3, 'z', PAIR_CYAN);
        add_pixel(kitty, cx + 6, cy - 4, 'Z', PAIR_CYAN);
    }
}

void generate_playing_kitty(Kitty* kitty) {
    int cx = kitty->cx;
    int cy = kitty->cy;
    
    // Pouncing position
    // Back legs
    add_pixel(kitty, cx - 4, cy + 2, '/', PAIR_ORANGE);
    add_pixel(kitty, cx - 3, cy + 2, '_', PAIR_ORANGE);
    add_pixel(kitty, cx - 2, cy + 2, '\\', PAIR_ORANGE);
    
    // Body stretched
    for (int i = -2; i <= 3; i++) {
        add_pixel(kitty, cx + i, cy, '=', PAIR_ORANGE);
        if (rand() % 100 < 50) {
            add_pixel(kitty, cx + i, cy - 1, '*', PAIR_ORANGE);
        }
    }
    
    // Front paws up
    add_pixel(kitty, cx + 4, cy - 1, '\\', PAIR_WHITE);
    add_pixel(kitty, cx + 5, cy - 2, '\\', PAIR_WHITE);
    add_pixel(kitty, cx + 4, cy, '/', PAIR_WHITE);
    add_pixel(kitty, cx + 5, cy - 1, '/', PAIR_WHITE);
    
    // Excited tail
    generate_tail(kitty, cx - 4, cy - 1, 12, 2.0);
    
    // Alert head
    kitty->ear_perk = 1;
    generate_head(kitty);
}

void generate_yarn_ball(Kitty* kitty, int x, int y) {
    // Yarn ball
    add_pixel(kitty, x - 1, y - 1, '/', PAIR_PINK);
    add_pixel(kitty, x, y - 1, '@', PAIR_PINK);
    add_pixel(kitty, x + 1, y - 1, '\\', PAIR_PINK);
    add_pixel(kitty, x - 1, y, '@', PAIR_PINK);
    add_pixel(kitty, x, y, '@', PAIR_PINK);
    add_pixel(kitty, x + 1, y, '@', PAIR_PINK);
    add_pixel(kitty, x, y + 1, '@', PAIR_PINK);
    
    // Yarn string
    for (int i = 1; i < 5; i++) {
        add_pixel(kitty, x + i + 1, y + (i % 2), '~', PAIR_PINK);
    }
}

void generate_mouse(Kitty* kitty, int x, int y) {
    add_pixel(kitty, x, y, '<', PAIR_GRAY);
    add_pixel(kitty, x + 1, y, ':', PAIR_GRAY);
    add_pixel(kitty, x + 2, y, '3', PAIR_GRAY);
    add_pixel(kitty, x + 3, y, '~', PAIR_GRAY);
    add_pixel(kitty, x + 4, y, '~', PAIR_GRAY);
}

void generate_bird(Kitty* kitty, int x, int y) {
    add_pixel(kitty, x - 1, y, 'v', PAIR_YELLOW);
    add_pixel(kitty, x, y, '^', PAIR_YELLOW);
    add_pixel(kitty, x + 1, y, 'v', PAIR_YELLOW);
}

void generate_kitty(Kitty* kitty) {
    kitty->pixel_count = 0;
    
    switch (kitty->pose) {
        case KITTY_SITTING:
            generate_tail(kitty, kitty->cx - 3, kitty->cy + 1, 15, 1.5);
            generate_body_sitting(kitty);
            generate_head(kitty);
            break;
            
        case KITTY_SLEEPING:
            kitty->eye_state = 2;
            generate_sleeping_kitty(kitty);
            break;
            
        case KITTY_PLAYING:
            kitty->eye_state = 0;
            generate_playing_kitty(kitty);
            // Add toys
            if (rand() % 100 < 50) {
                generate_yarn_ball(kitty, kitty->cx + 8, kitty->cy + 2);
            }
            if (rand() % 100 < 30) {
                generate_mouse(kitty, kitty->cx + 10, kitty->cy + 3);
            }
            break;
            
        case KITTY_WALKING:
            // TODO: Implement walking animation
            generate_body_sitting(kitty);
            generate_head(kitty);
            generate_tail(kitty, kitty->cx - 3, kitty->cy + 1, 15, 0.5);
            break;
    }
    
    // Random environment elements
    if (rand() % 100 < 20) {
        int bx = kitty->cx + (rand() % 20) - 10;
        int by = kitty->cy - 5 - (rand() % 3);
        generate_bird(kitty, bx, by);
    }
}

void animate_kitty(Kitty* kitty) {
    kitty->animation_frame++;
    
    // Eye blinking
    if (kitty->pose != KITTY_SLEEPING) {
        if (kitty->animation_frame % 100 == 0) {
            kitty->eye_state = 1;
        } else if (kitty->animation_frame % 100 == 5) {
            kitty->eye_state = 0;
        }
    }
    
    // Whisker twitching
    if (kitty->animation_frame % 60 == 0) {
        kitty->whisker_twitch = !kitty->whisker_twitch;
    }
    
    // Ear perking
    if (kitty->pose == KITTY_PLAYING) {
        kitty->ear_perk = 1;
    } else if (kitty->animation_frame % 200 == 0) {
        kitty->ear_perk = rand() % 2;
    }
    
    // Regenerate with animations
    generate_kitty(kitty);
}

void draw_kitty(Kitty* kitty, Config* config) {
    for (int i = 0; i < kitty->pixel_count; i++) {
        Pixel* p = &kitty->pixels[i];
        if (!p->visible) continue;
        
        if (config->colors) {
            int color = p->color;
            if (config->rainbow) {
                color = (kitty->animation_frame / 10 + i) % 8 + 1;
            }
            attron(COLOR_PAIR(color));
        }
        
        mvaddch(p->y, p->x, p->ch);
        
        if (config->colors) {
            attroff(COLOR_PAIR(p->color));
        }
    }
}

void draw_ground(int max_x, int max_y) {
    int ground_y = max_y - 5;
    for (int x = 0; x < max_x; x++) {
        if (x % 4 == 0) {
            mvaddch(ground_y, x, '_');
        }
    }
}

void print_usage(void) {
    printf("ckitty v3 - Advanced procedural kitty generator\n\n");
    printf("Usage: ckitty [OPTIONS]\n\n");
    printf("OPTIONS:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -l, --live          Live generation mode\n");
    printf("  -d, --delay <ms>    Animation delay (default: 40000)\n");
    printf("  -c, --colors        Enable colors\n");
    printf("  -r, --rainbow       Rainbow mode\n");
    printf("  -s, --seed <num>    Random seed\n");
    printf("  -i, --infinite      Run indefinitely\n");
    printf("  -S, --screensaver   Screensaver mode\n");
    printf("  -m, --message <msg> Custom message\n");
}

int main(int argc, char *argv[]) {
    Config config = {
        .delay = DELAY_DEFAULT,
        .grow_delay = GROW_DELAY,
        .live = 0,
        .colors = 0,
        .screensaver = 0,
        .infinite = 0,
        .rainbow = 0,
        .seed = time(NULL),
        .message = NULL
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
        {"message", required_argument, 0, 'm'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hld:crs:iSm:", long_options, NULL)) != -1) {
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
            case 'm':
                config.message = optarg;
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
    kitty.cx = max_x / 2;
    kitty.cy = max_y / 2;
    kitty.pose = rand() % 4;
    kitty.direction = (rand() % 2) * 2 - 1;
    
    int ch;
    int running = 1;
    
    while (running) {
        clear();
        
        draw_ground(max_x, max_y);
        
        animate_kitty(&kitty);
        draw_kitty(&kitty, &config);
        
        // Display message if provided
        if (config.message) {
            mvprintw(2, (max_x - strlen(config.message)) / 2, "%s", config.message);
        }
        
        // Display seed
        mvprintw(max_y - 1, 1, "Seed: %d", config.seed);
        
        refresh();
        
        ch = getch();
        if (ch == 'q' || ch == 27) {
            running = 0;
        } else if (ch == ' ') {
            // Change pose
            kitty.pose = (kitty.pose + 1) % 4;
        } else if (ch == 'n' && config.screensaver) {
            // New kitty
            kitty.cx = rand() % (max_x - 20) + 10;
            kitty.cy = rand() % (max_y - 10) + 5;
            kitty.pose = rand() % 4;
            kitty.direction = (rand() % 2) * 2 - 1;
            kitty.animation_frame = 0;
        }
        
        if (!config.infinite && kitty.animation_frame > 500) {
            running = 0;
        }
        
        usleep(config.delay);
    }
    
    endwin();
    return 0;
}
