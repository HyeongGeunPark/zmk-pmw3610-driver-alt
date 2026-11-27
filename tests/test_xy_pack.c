/*
 * SPDX-License-Identifier: MIT
 *
 * Host test for pmw3610_xy_pack.h. Models the path
 *   driver report -> one notification per event -> unpack device
 * and checks that the motion that comes out equals the motion that went in,
 * and how many notifications it took.
 *
 *   clang -Wall -Wextra -Werror -I../src test_xy_pack.c -o test_xy_pack
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pmw3610_xy_pack.h"

#define CODE_X 0x00 /* INPUT_REL_X */
#define CODE_Y 0x01 /* INPUT_REL_Y */

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

struct link {
    bool packed_xy; /* the devicetree property */
    long sent;      /* events the driver reported = notifications over a split */
    long out_x;     /* motion seen by the listener behind the unpack device */
    long out_y;
    long out_syncs; /* frames the listener reported */
    bool open_x;    /* an X with sync = false is waiting for its Y */
};

/* What pmw3610_xy_unpack_forward() does with one event. */
static void unpack_forward(struct link *l, uint16_t code, int32_t value, bool sync) {
    if (code == PMW3610_XY_PACKED_CODE) {
        int16_t x, y;

        pmw3610_xy_unpack(value, &x, &y);
        /* X with sync = false, then Y with the sync of the packed event. */
        l->out_x += x;
        l->out_y += y;
        l->out_syncs += sync ? 1 : 0;
        l->open_x = !sync;
        return;
    }

    if (code == CODE_X) {
        l->out_x += value;
        l->open_x = !sync;
    } else if (code == CODE_Y) {
        l->out_y += value;
        l->open_x = false;
    }
    l->out_syncs += sync ? 1 : 0;
}

static void report(struct link *l, uint16_t code, int32_t value, bool sync) {
    l->sent++;
    unpack_forward(l, code, value, sync);
}

/* What pmw3610_report_data() does with one frame. */
static void driver_frame(struct link *l, int16_t rx, int16_t ry) {
    bool have_x = rx != 0;
    bool have_y = ry != 0;

    if (!(have_x || have_y)) {
        return;
    }

    if (l->packed_xy && have_x && have_y) {
        report(l, PMW3610_XY_PACKED_CODE, pmw3610_xy_pack(rx, ry), true);
    } else {
        if (have_x) {
            report(l, CODE_X, rx, !have_y);
        }
        if (have_y) {
            report(l, CODE_Y, ry, true);
        }
    }
}

static void test_pack_unpack(void) {
    static const int16_t v[] = {0, 1, -1, 2, -2, 127, -128, 255, -256, 2047, -2048, 32767, -32768};
    int16_t x, y;

    for (size_t i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        for (size_t j = 0; j < sizeof(v) / sizeof(v[0]); j++) {
            pmw3610_xy_unpack(pmw3610_xy_pack(v[i], v[j]), &x, &y);
            CHECK(x == v[i]);
            CHECK(y == v[j]);
        }
    }
}

static void test_both_axes_is_one_event(void) {
    struct link l = {.packed_xy = true};

    driver_frame(&l, 5, -3);
    CHECK(l.sent == 1);
    CHECK(l.out_x == 5 && l.out_y == -3);
    CHECK(l.out_syncs == 1);
    CHECK(!l.open_x);
}

static void test_single_axis_is_unchanged(void) {
    struct link l = {.packed_xy = true};

    /* X only: goes out at once, as the last event of its frame. */
    driver_frame(&l, 7, 0);
    CHECK(l.sent == 1);
    CHECK(l.out_x == 7 && l.out_y == 0 && l.out_syncs == 1);

    /* Many X-only frames in a row: one event each, nothing held back. */
    for (int i = 0; i < 100; i++) {
        driver_frame(&l, 1, 0);
    }
    CHECK(l.sent == 101);
    CHECK(l.out_x == 107 && l.out_syncs == 101);

    driver_frame(&l, 0, -9);
    CHECK(l.sent == 102);
    CHECK(l.out_y == -9 && l.out_syncs == 102);

    driver_frame(&l, 0, 0);
    CHECK(l.sent == 102);
}

static void test_property_off_is_unchanged(void) {
    struct link l = {.packed_xy = false};

    driver_frame(&l, 5, -3);
    CHECK(l.sent == 2);
    CHECK(l.out_x == 5 && l.out_y == -3 && l.out_syncs == 1);
}

static void test_random_stream(void) {
    struct link packed = {.packed_xy = true};
    struct link plain = {.packed_xy = false};
    long in_x = 0, in_y = 0, frames_with_motion = 0, both_axes = 0;

    srand(12345);
    for (int i = 0; i < 200000; i++) {
        /* 12-bit sensor deltas, with zeros common on each axis. */
        int16_t x = (rand() % 4 == 0) ? 0 : (int16_t)((rand() % 4096) - 2048);
        int16_t y = (rand() % 4 == 0) ? 0 : (int16_t)((rand() % 4096) - 2048);

        driver_frame(&packed, x, y);
        driver_frame(&plain, x, y);
        in_x += x;
        in_y += y;
        if (x != 0 || y != 0) {
            frames_with_motion++;
        }
        if (x != 0 && y != 0) {
            both_axes++;
        }
    }

    CHECK(packed.out_x == in_x && packed.out_y == in_y);
    CHECK(plain.out_x == in_x && plain.out_y == in_y);
    /* One event and one report per frame that moved. */
    CHECK(packed.sent == frames_with_motion);
    CHECK(packed.out_syncs == frames_with_motion);
    CHECK(plain.sent == frames_with_motion + both_axes);
    CHECK(plain.out_syncs == frames_with_motion);
    CHECK(!packed.open_x && !plain.open_x);

    printf("random stream: %ld frames with motion, %ld on both axes\n", frames_with_motion,
           both_axes);
    printf("  events without packed-xy: %ld\n", plain.sent);
    printf("  events with packed-xy:    %ld\n", packed.sent);
}

int main(void) {
    test_pack_unpack();
    test_both_axes_is_one_event();
    test_single_axis_is_unchanged();
    test_property_off_is_unchanged();
    test_random_stream();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
