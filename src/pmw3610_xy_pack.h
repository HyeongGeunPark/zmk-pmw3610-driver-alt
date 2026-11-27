/*
 * SPDX-License-Identifier: MIT
 *
 * One input event for a frame that moved on both axes.
 *
 * Normally the driver reports such a frame as an X event followed by a Y
 * event. Over a ZMK split each input event is its own notification, so the
 * frame costs two. With the `packed-xy` property the driver reports one
 * relative event instead, with code PMW3610_XY_PACKED_CODE and a 32-bit value
 * that holds X in the upper and Y in the lower 16 bits. Frames that moved on
 * one axis are reported as before.
 *
 * Nothing but the pmw3610-alt-xy-unpack device understands the packed event,
 * so the side that receives it has to route it through one.
 *
 * This header has no Zephyr dependency, so it can be tested on a host.
 */

#pragma once

#include <stdint.h>

/* Same value as INPUT_REL_MISC; the sources that use it assert that. */
#define PMW3610_XY_PACKED_CODE 0x09

static inline int32_t pmw3610_xy_pack(int16_t x, int16_t y) {
    return (int32_t)(((uint32_t)(uint16_t)x << 16) | (uint32_t)(uint16_t)y);
}

static inline void pmw3610_xy_unpack(int32_t packed, int16_t *x, int16_t *y) {
    uint32_t v = (uint32_t)packed;

    *x = (int16_t)(uint16_t)(v >> 16);
    *y = (int16_t)(uint16_t)(v & 0xFFFFu);
}
