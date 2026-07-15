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
    expect_str_contains(s1, "/\\_/\\", "kitty head must contain /\\_/\\");
    expect_str_contains(s1, "(_)", "kitty must contain paws");

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
    ckitty_canvas c = {0};
    expect_true(ckitty_canvas_init(&c, 10, 5), "canvas_init small");

    ckitty_kitty k = {
        .seed = 7u,
        .pose = CKPOSE_SIT,
        .facing = 1,
        .cx = 5,
        .cy = 2,
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
        .has_bird = 0
    };

    // Should not crash or write out of bounds (canvas_set clamps).
    ckitty_render_frame(&k, 0, &c);
    (void)count_non_space(&c);
    ckitty_canvas_free(&c);
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
    ckitty_canvas_copy_visible(&full, &order, order.len, &visible);
    char* full_dump = ckitty_canvas_dump_bbox(&full);
    char* visible_dump = ckitty_canvas_dump_bbox(&visible);
    expect_str_eq(full_dump, visible_dump, "full visible copy must match source");
    free(full_dump);
    free(visible_dump);
    ckitty_draw_order_free(&order);
    ckitty_canvas_free(&full);
    ckitty_canvas_free(&visible);
}

int main(void) {
    test_rng_and_canvas_contract();
    test_determinism_and_content();
    test_animation_changes_frame();
    test_draw_order_unique_and_complete();
    test_small_canvas_safe();
    test_all_poses_and_large_frame();
    test_randomize_and_visible_copy();

    printf("OK\n");
    return 0;
}
