#include "../src/ckitty_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char* msg) {
    fprintf(stderr, "TEST FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(int cond, const char* msg) {
    if (!cond) fail(msg);
}

static void expect_str_contains(const char* haystack, const char* needle, const char* msg) {
    if (!haystack || !needle || !strstr(haystack, needle)) fail(msg);
}

static void expect_str_eq(const char* a, const char* b, const char* msg) {
    if (!a || !b || strcmp(a, b) != 0) fail(msg);
}

static void expect_str_neq(const char* a, const char* b, const char* msg) {
    if (!a || !b) fail("null string in expect_str_neq");
    if (strcmp(a, b) == 0) fail(msg);
}

static void test_rng_and_canvas_contract(void) {
    ckitty_rng a;
    ckitty_rng b;
    ckitty_rng_seed(&a, 0);
    ckitty_rng_seed(&b, 0);
    for (int i = 0; i < 16; i++) {
        expect_true(ckitty_rng_u32(&a) == ckitty_rng_u32(&b), "zero seed must be deterministic");
    }
    expect_true(ckitty_rng_range(&a, 0) == 0, "zero range must be safe");
    expect_true(ckitty_rng_range(&a, -1) == 0, "negative range must be safe");

    ckitty_canvas c = {0};
    expect_true(!ckitty_canvas_init(&c, 0, 4), "zero width must fail");
    expect_true(!ckitty_canvas_init(&c, 4001, 1000), "oversized canvas must fail safely");
    expect_true(ckitty_canvas_init(&c, 4, 3), "small canvas init");
    ckitty_canvas_set(&c, 1, 1, 'X', CKCLR_TOY);
    expect_true(ckitty_canvas_get(&c, 1, 1) == 'X', "canvas get after set");
    expect_true(ckitty_canvas_get_color(&c, 1, 1) == CKCLR_TOY, "canvas color after set");
    ckitty_canvas_set(&c, -1, -1, '!', CKCLR_NOSE);
    expect_true(ckitty_canvas_get(&c, -1, -1) == ' ', "out of bounds get must be blank");
    ckitty_canvas_set(&c, 1, 1, ' ', CKCLR_TOY);
    expect_true(ckitty_canvas_get_color(&c, 1, 1) == CKCLR_NONE, "blank cells have no color");
    ckitty_canvas_free(&c);
}

static int count_non_space(const ckitty_canvas* c) {
    int count = 0;
    for (int y = 0; y < c->h; y++) {
        for (int x = 0; x < c->w; x++) {
            if (ckitty_canvas_get(c, x, y) != ' ') count++;
        }
    }
    return count;
}

static void test_determinism_and_content(void) {
    ckitty_canvas c1 = {0};
    ckitty_canvas c2 = {0};
    expect_true(ckitty_canvas_init(&c1, 80, 24), "canvas_init c1");
    expect_true(ckitty_canvas_init(&c2, 80, 24), "canvas_init c2");

    // Fixed kitty so the test doesn't depend on randomization details.
    ckitty_kitty k = {
        .seed = 123u,
        .pose = CKPOSE_SIT,
        .facing = 1,
        .cx = 40,
        .cy = 12,
        .body_w = 9,
        .body_h = 4,
        .fur_density = 70,
        .fur_a = '*',
        .fur_b = '.',
        .tail_len = 14,
        .tail_amp = 1.2,
        .tail_phase = 0.25,
        .eye_style = 0,
        .mouth_style = 0,
        .has_yarn = 0,
        .has_mouse = 0,
        .has_bird = 1,
        .bird_dx = 6,
        .bird_dy = -2
    };

    ckitty_render_frame(&k, 0, &c1);
    ckitty_render_frame(&k, 0, &c2);

    char* s1 = ckitty_canvas_dump_bbox(&c1);
    char* s2 = ckitty_canvas_dump_bbox(&c2);
    expect_true(s1 && s2, "bbox dump allocation");

    expect_str_eq(s1, s2, "rendering same frame must be deterministic");
    expect_str_contains(s1, "/\\___/\\", "kitty head must retain its two upright ears");
    expect_str_contains(s1, "(__)(__)", "sitting kitty must have paired forepaws");

    free(s1);
    free(s2);
    ckitty_canvas_free(&c1);
    ckitty_canvas_free(&c2);
}

static void test_animation_changes_frame(void) {
    ckitty_canvas c1 = {0};
    ckitty_canvas c2 = {0};
    expect_true(ckitty_canvas_init(&c1, 80, 24), "canvas_init c1");
    expect_true(ckitty_canvas_init(&c2, 80, 24), "canvas_init c2");

    ckitty_kitty k = {
        .seed = 999u,
        .pose = CKPOSE_SIT,
        .facing = -1,
        .cx = 40,
        .cy = 12,
        .body_w = 11,
        .body_h = 4,
        .fur_density = 65,
        .fur_a = '+',
        .fur_b = '.',
        .tail_len = 16,
        .tail_amp = 1.4,
        .tail_phase = 1.0,
        .eye_style = 1,
        .mouth_style = 2,
        .has_yarn = 0,
        .has_mouse = 0,
        .has_bird = 0
    };

    ckitty_render_frame(&k, 0, &c1);
    ckitty_render_frame(&k, 40, &c2);

    char* s1 = ckitty_canvas_dump_bbox(&c1);
    char* s2 = ckitty_canvas_dump_bbox(&c2);
    expect_true(s1 && s2, "bbox dump allocation");
    expect_str_neq(s1, s2, "different frames should differ (tail sway / blink / twitch)");

    free(s1);
    free(s2);
    ckitty_canvas_free(&c1);
    ckitty_canvas_free(&c2);
}

static void test_draw_order_unique_and_complete(void) {
    ckitty_canvas full = {0};
    expect_true(ckitty_canvas_init(&full, 80, 24), "canvas_init full");

    ckitty_kitty k = {
        .seed = 42u,
        .pose = CKPOSE_PLAY,
        .facing = 1,
        .cx = 35,
        .cy = 12,
        .body_w = 9,
        .body_h = 4,
        .fur_density = 70,
        .fur_a = '*',
        .fur_b = ':',
        .tail_len = 12,
        .tail_amp = 1.0,
        .tail_phase = 0.0,
        .eye_style = 0,
        .mouth_style = 0,
        .has_yarn = 1,
        .has_mouse = 1,
        .has_bird = 1,
        .bird_dx = -5,
        .bird_dy = -3
    };

    ckitty_render_frame(&k, 0, &full);
    int expected = count_non_space(&full);

    ckitty_draw_order order = {0};
    expect_true(ckitty_draw_order_build(&full, k.seed, &order), "draw_order_build");
    expect_true(order.len == expected, "draw_order length must match number of non-space cells");

    int total = full.w * full.h;
    unsigned char* seen = (unsigned char*)calloc((size_t)total, 1);
    expect_true(seen != NULL, "seen allocation");
    for (int i = 0; i < order.len; i++) {
        int idx = order.idx[i];
        expect_true(idx >= 0 && idx < total, "draw_order index out of range");
        expect_true(seen[idx] == 0, "draw_order contains duplicate indices");
        seen[idx] = 1;
    }

    free(seen);
    ckitty_draw_order_free(&order);
    ckitty_canvas_free(&full);
}

static void test_small_canvas_safe(void) {
    ckitty_canvas small = {0};
    ckitty_canvas full = {0};
    expect_true(ckitty_canvas_init(&small, 10, 5), "canvas_init small");
    expect_true(ckitty_canvas_init(&full, 100, 40), "canvas_init clipping reference");

    ckitty_rng rng;
    ckitty_kitty k;
    ckitty_rng_seed(&rng, 7u);
    ckitty_kitty_randomize(&k, &rng, 5, 2);
    k.has_yarn = k.has_mouse = k.has_bird = 0;
    const uint64_t frames[] = {0, 1, 8, 20, 40, UINT64_MAX};
    const int anchors[][2] = {{0, 0}, {5, 2}, {9, 4}, {-8, -3}, {17, 8}};

    // Every clipped cell must equal the same part of an unclipped rendering.
    // Sanitizer runs also check the canvas allocations around all four edges.
    for (int pose = CKPOSE_SIT; pose <= CKPOSE_WALK; pose++) {
        k.pose = (ckitty_pose)pose;
        for (int facing = -1; facing <= 1; facing += 2) {
            k.facing = facing;
            for (size_t a = 0; a < sizeof(anchors) / sizeof(anchors[0]); a++) {
                k.cx = anchors[a][0];
                k.cy = anchors[a][1];
                ckitty_kitty reference = k;
                reference.cx += 40;
                reference.cy += 16;
                for (size_t f = 0; f < sizeof(frames) / sizeof(frames[0]); f++) {
                    ckitty_render_frame(&k, frames[f], &small);
                    ckitty_render_frame(&reference, frames[f], &full);
                    for (int y = 0; y < small.h; y++) {
                        for (int x = 0; x < small.w; x++) {
                            expect_true(ckitty_canvas_get(&small, x, y) ==
                                            ckitty_canvas_get(&full, x + 40, y + 16),
                                        "small canvas clips the same silhouette safely");
                            expect_true(ckitty_canvas_get_color(&small, x, y) ==
                                            ckitty_canvas_get_color(&full, x + 40, y + 16),
                                        "clipping preserves every visible cell color");
                        }
                    }
                }
            }
        }
    }
    ckitty_canvas_free(&small);
    ckitty_canvas_free(&full);
}

static void test_all_poses_and_large_frame(void) {
    ckitty_canvas c = {0};
    expect_true(ckitty_canvas_init(&c, 80, 24), "canvas_init all poses");

    ckitty_kitty k = {
        .seed = 1234u,
        .pose = CKPOSE_SIT,
        .facing = 1,
        .cx = 40,
        .cy = 12,
        .body_w = 9,
        .body_h = 4,
        .fur_density = 70,
        .fur_a = '*',
        .fur_b = '.',
        .tail_len = 14,
        .tail_amp = 1.2,
        .tail_phase = 0.25,
        .eye_style = 1,
        .mouth_style = 2,
        .has_yarn = 1,
        .has_mouse = 1,
        .has_bird = 1,
        .bird_dx = 5,
        .bird_dy = -2
    };

    for (int pose = CKPOSE_SIT; pose <= CKPOSE_WALK; pose++) {
        k.pose = (ckitty_pose)pose;
        ckitty_render_frame(&k, UINT64_MAX, &c);
        char* dump = ckitty_canvas_dump_bbox(&c);
        expect_true(dump && dump[0] != '\0', "every pose renders at a large frame");
        free(dump);
    }
    ckitty_canvas_free(&c);
}

static void test_randomize_and_visible_copy(void) {
    ckitty_rng r1;
    ckitty_rng r2;
    ckitty_kitty k1;
    ckitty_kitty k2;
    ckitty_rng_seed(&r1, 9876u);
    ckitty_rng_seed(&r2, 9876u);
    ckitty_kitty_randomize(&k1, &r1, 20, 10);
    ckitty_kitty_randomize(&k2, &r2, 20, 10);
    expect_true(memcmp(&k1, &k2, sizeof(k1)) == 0, "randomization must be reproducible");

    ckitty_canvas full = {0};
    ckitty_canvas visible = {0};
    expect_true(ckitty_canvas_init(&full, 40, 20), "full copy canvas init");
    expect_true(ckitty_canvas_init(&visible, 40, 20), "visible copy canvas init");
    ckitty_render_frame(&k1, 0, &full);
    ckitty_draw_order order = {0};
    expect_true(ckitty_draw_order_build(&full, k1.seed, &order), "copy draw order build");
    ckitty_canvas_copy_visible(&full, &order, 0, &visible);
    expect_true(count_non_space(&visible) == 0, "zero visible cells must be blank");
    int halfway = order.len / 2;
    ckitty_canvas_copy_visible(&full, &order, halfway, &visible);
    expect_true(count_non_space(&visible) == halfway, "partial reveal shows exactly its requested cells");
    for (int i = 0; i < order.len; i++) {
        int idx = order.idx[i];
        expect_true(visible.ch[idx] == ((i < halfway) ? full.ch[idx] : ' '),
                    "partial reveal follows the seeded draw order");
        expect_true(visible.color[idx] == ((i < halfway) ? full.color[idx] : CKCLR_NONE),
                    "partial reveal preserves colors only for revealed cells");
    }
    ckitty_canvas_copy_visible(&full, &order, order.len, &visible);
    char* full_dump = ckitty_canvas_dump_bbox(&full);
    char* visible_dump = ckitty_canvas_dump_bbox(&visible);
    expect_str_eq(full_dump, visible_dump, "full visible copy must match source");
    expect_true(memcmp(full.color, visible.color, (size_t)full.w * (size_t)full.h) == 0,
                "full reveal preserves the source colors");
    free(full_dump);
    free(visible_dump);
    ckitty_draw_order_free(&order);
    ckitty_canvas_free(&full);
    ckitty_canvas_free(&visible);
}

static ckitty_kitty seeded_kitty(uint32_t seed, ckitty_pose pose) {
    ckitty_rng rng;
    ckitty_kitty k;
    ckitty_rng_seed(&rng, seed);
    ckitty_kitty_randomize(&k, &rng, 50, 20);
    k.pose = pose;
    return k;
}

static void expect_separate_decoration(const ckitty_canvas* bare, const ckitty_canvas* decorated) {
    int added = 0;
    for (int y = 0; y < bare->h; y++) {
        for (int x = 0; x < bare->w; x++) {
            char before = ckitty_canvas_get(bare, x, y);
            char after = ckitty_canvas_get(decorated, x, y);
            if (before != ' ') {
                expect_true(before == after, "toy must not replace any cat cell");
                expect_true(ckitty_canvas_get_color(bare, x, y) ==
                                ckitty_canvas_get_color(decorated, x, y),
                            "toy must not recolor any cat cell");
            } else if (after != ' ') {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        expect_true(ckitty_canvas_get(bare, x + dx, y + dy) == ' ',
                                    "toy silhouette stays visibly separate from the cat");
                    }
                }
                added++;
            }
        }
    }
    expect_true(added > 0, "enabled toy must remain visible");
}

static void test_play_toys_do_not_overlap(void) {
    ckitty_canvas both = {0};
    ckitty_canvas yarn = {0};
    ckitty_canvas bare = {0};
    expect_true(ckitty_canvas_init(&both, 100, 40), "combined toy canvas init");
    expect_true(ckitty_canvas_init(&yarn, 100, 40), "yarn canvas init");
    expect_true(ckitty_canvas_init(&bare, 100, 40), "bare cat canvas init");
    ckitty_kitty k = seeded_kitty(123u, CKPOSE_PLAY);
    expect_true(k.has_yarn && k.has_mouse, "seed 123 exercises both toys");
    k.has_bird = 0;
    size_t cells = (size_t)both.w * (size_t)both.h;

    for (int facing = -1; facing <= 1; facing += 2) {
        k.facing = facing;
        for (uint64_t frame = 0; frame < 40; frame++) {
            ckitty_kitty yarn_only = k;
            yarn_only.has_mouse = 0;
            ckitty_kitty no_toys = k;
            no_toys.has_yarn = no_toys.has_mouse = 0;
            ckitty_render_frame(&no_toys, frame, &bare);
            ckitty_render_frame(&k, frame, &both);
            ckitty_render_frame(&yarn_only, frame, &yarn);
            expect_separate_decoration(&bare, &both);
            expect_true(memcmp(both.ch, yarn.ch, cells) == 0,
                        "yarn takes precedence so a mouse cannot overwrite it");
            expect_true(memcmp(both.color, yarn.color, cells) == 0,
                        "mouse cannot recolor the yarn");
        }
        ckitty_kitty mouse_only = k;
        mouse_only.has_yarn = 0;
        ckitty_kitty no_toys = mouse_only;
        no_toys.has_mouse = 0;
        for (uint64_t frame = 0; frame < 40; frame++) {
            ckitty_render_frame(&no_toys, frame, &bare);
            ckitty_render_frame(&mouse_only, frame, &both);
            expect_separate_decoration(&bare, &both);
        }
        char* dump = ckitty_canvas_dump_bbox(&both);
        expect_str_contains(dump, "<:3~~", "mouse is visible when yarn is absent");
        free(dump);
    }
    ckitty_canvas_free(&both);
    ckitty_canvas_free(&yarn);
    ckitty_canvas_free(&bare);
}

static int is_kitty_cell(const ckitty_canvas* c, int x, int y) {
    uint8_t color = ckitty_canvas_get_color(c, x, y);
    return color == CKCLR_FUR || color == CKCLR_PAW ||
           color == CKCLR_NOSE || color == CKCLR_GRAY;
}

static void test_compact_silhouettes_and_face(void) {
    ckitty_canvas c = {0};
    expect_true(ckitty_canvas_init(&c, 100, 40), "silhouette canvas init");
    const uint32_t seeds[] = {1u, 2u, 123u, 999u, 1234u};
    // Bounds include the play pose's one-row bob. Sleep z's are decoration,
    // so only feline cells contribute to the silhouette's envelope.
    const int top[] = {-3, -3, -3, -2};
    const int bottom[] = {4, 3, 5, 4};
    const int heights[] = {8, 7, 7, 7};

    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
        ckitty_kitty k = seeded_kitty(seeds[s], CKPOSE_SIT);
        k.has_yarn = k.has_mouse = k.has_bird = 0;
        for (int pose = CKPOSE_SIT; pose <= CKPOSE_WALK; pose++) {
            k.pose = (ckitty_pose)pose;
            for (int facing = -1; facing <= 1; facing += 2) {
                k.facing = facing;
                for (uint64_t frame = 0; frame < 96; frame++) {
                    ckitty_render_frame(&k, frame, &c);
                    char* dump = ckitty_canvas_dump_bbox(&c);
                    expect_str_contains(dump, "/\\___/\\", "every pose retains paired upright ears");
                    if (k.pose == CKPOSE_SIT) {
                        expect_str_contains(dump, "(__)(__)", "sitting forepaws remain paired through animation");
                    }
                    free(dump);

                    int min_x = c.w, min_y = c.h, max_x = -1, max_y = -1;
                    int noses = 0;
                    for (int y = 0; y < c.h; y++) {
                        for (int x = 0; x < c.w; x++) {
                            if (!is_kitty_cell(&c, x, y)) continue;
                            if (x < min_x) min_x = x;
                            if (x > max_x) max_x = x;
                            if (y < min_y) min_y = y;
                            if (y > max_y) max_y = y;
                            if (ckitty_canvas_get_color(&c, x, y) == CKCLR_NOSE) {
                                expect_true(ckitty_canvas_get(&c, x, y) == '^',
                                            "nose has a compact feline triangle");
                                noses++;
                            }
                        }
                    }
                    expect_true(noses == 1, "every pose retains one visible nose");
                    expect_true(max_x - min_x + 1 <= 32, "cat silhouette stays compact without a rope tail");
                    expect_true(max_y - min_y + 1 == heights[pose], "pose retains its complete silhouette height");
                    expect_true(min_y >= k.cy + top[pose] && max_y <= k.cy + bottom[pose],
                                "pose motion stays within the terminal layout envelope");
                }
            }
        }
    }
    ckitty_canvas_free(&c);
}

static void test_all_poses_animate(void) {
    ckitty_canvas first = {0};
    ckitty_canvas later = {0};
    expect_true(ckitty_canvas_init(&first, 100, 40), "first animation canvas init");
    expect_true(ckitty_canvas_init(&later, 100, 40), "later animation canvas init");
    ckitty_kitty k = seeded_kitty(999u, CKPOSE_SIT);
    k.has_yarn = k.has_mouse = k.has_bird = 0;
    size_t cells = (size_t)first.w * (size_t)first.h;

    for (int pose = CKPOSE_SIT; pose <= CKPOSE_WALK; pose++) {
        k.pose = (ckitty_pose)pose;
        for (int facing = -1; facing <= 1; facing += 2) {
            k.facing = facing;
            ckitty_render_frame(&k, 0, &first);
            int changed = 0;
            for (uint64_t frame = 1; frame < 96; frame++) {
                ckitty_render_frame(&k, frame, &later);
                if (memcmp(first.ch, later.ch, cells) != 0) changed = 1;
            }
            expect_true(changed, "every pose animates within a complete motion cycle");
        }
    }
    ckitty_canvas_free(&first);
    ckitty_canvas_free(&later);
}

static void test_birds_do_not_strobe(void) {
    ckitty_canvas with_bird = {0};
    ckitty_canvas without_bird = {0};
    expect_true(ckitty_canvas_init(&with_bird, 100, 40), "bird canvas init");
    expect_true(ckitty_canvas_init(&without_bird, 100, 40), "bird baseline canvas init");
    ckitty_kitty k = seeded_kitty(2u, CKPOSE_SLEEP);
    expect_true(k.has_bird, "seed 2 exercises the bird");
    k.has_yarn = k.has_mouse = 0;

    for (int pose = CKPOSE_SIT; pose <= CKPOSE_WALK; pose++) {
        k.pose = (ckitty_pose)pose;
        for (int facing = -1; facing <= 1; facing += 2) {
            k.facing = facing;
            ckitty_kitty baseline = k;
            baseline.has_bird = 0;
            for (uint64_t frame = 0; frame < 80; frame++) {
                ckitty_render_frame(&k, frame, &with_bird);
                ckitty_render_frame(&baseline, frame, &without_bird);
                int bird_cells = 0;
                for (int y = 0; y < with_bird.h; y++) {
                    for (int x = 0; x < with_bird.w; x++) {
                        char before = ckitty_canvas_get(&without_bird, x, y);
                        char after = ckitty_canvas_get(&with_bird, x, y);
                        if (before == after && ckitty_canvas_get_color(&without_bird, x, y) ==
                                               ckitty_canvas_get_color(&with_bird, x, y)) continue;
                        expect_true(before == ' ', "bird stays separate from the cat and sleep accents");
                        expect_true(after != ' ', "bird remains present between animation frames");
                        expect_true(ckitty_canvas_get_color(&with_bird, x, y) == CKCLR_ACCENT,
                                    "bird keeps its accent color");
                        bird_cells++;
                    }
                }
                expect_true(bird_cells == 3, "complete bird stays visible throughout every pose");
            }
        }
    }
    ckitty_canvas_free(&with_bird);
    ckitty_canvas_free(&without_bird);
}

int main(void) {
    test_rng_and_canvas_contract();
    test_determinism_and_content();
    test_animation_changes_frame();
    test_draw_order_unique_and_complete();
    test_small_canvas_safe();
    test_all_poses_and_large_frame();
    test_randomize_and_visible_copy();
    test_play_toys_do_not_overlap();
    test_compact_silhouettes_and_face();
    test_all_poses_animate();
    test_birds_do_not_strobe();

    printf("OK\n");
    return 0;
}
