/* SH4DER-PVR

   test/pvrx_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of pvrx's pure parts: the wait's count compare across a wrap; the
   download's alignment test, rounding and the two address words (the values the
   console ran with); the TA counters' difference across a wrap, the overflow
   flag and the any flag; the revision table (our console's reads give 2.42);
   the clip's tiles from pixels, its eight words and its refusals; the dummy
   triangle has area by the guard and lies off the screen's left edge. The
   Dreamcast side (the hooks, the registers) is not run here.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_pvrx.h"

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int wait_count(void) {
    if (sh4der_pvrx_wait_behind(0u, 0u)) return fail("nothing finished, still waiting");
    if (!sh4der_pvrx_wait_behind(0u, 1u)) return fail("one scene finished, none seen: not waiting");
    if (sh4der_pvrx_wait_behind(3u, 3u)) return fail("all seen, still waiting");
    if (sh4der_pvrx_wait_behind(4u, 3u)) return fail("one more seen than wanted (a resync), still waiting");
    if (!sh4der_pvrx_wait_behind(0xffffffffu, 0u)) return fail("the count wrapped: want is one ahead, not waiting");
    if (sh4der_pvrx_wait_behind(1u, 0xffffffffu)) return fail("the count wrapped: seen is two ahead, still waiting");
    return 0;
}

static int dma_words(void) {
    static uint8_t buf[256];
    const uintptr_t base = ((uintptr_t)buf + 31u) & ~(uintptr_t)31u;
    uint32_t n;

    if (!sh4der_pvrx_dma_ok((const void *)base, 32u)) return fail("an aligned 32-byte copy refused");
    if (!sh4der_pvrx_dma_ok((const void *)base, 131072u)) return fail("an aligned 128 KB copy refused");
    if (sh4der_pvrx_dma_ok((const void *)(base + 4u), 32u)) return fail("a destination 4 bytes off taken");
    if (sh4der_pvrx_dma_ok((const void *)(base + 16u), 32u)) return fail("a destination 16 bytes off taken");
    if (sh4der_pvrx_dma_ok((const void *)base, 0u)) return fail("a copy of nothing taken");
    if (sh4der_pvrx_dma_ok((const void *)base, 48u)) return fail("a length that is not whole units taken");
    if (sh4der_pvrx_dma_ok(NULL, 32u)) return fail("no destination taken");
    for (n = 0u; n < 200u; n++) {
        const uint32_t r = sh4der_pvrx_dma_round(n);

        if (r < n || r - n > 31u || (r & 31u)) return fail("round is not the next whole unit");
    }
    if (sh4der_pvrx_dma_round(32768u) != 32768u) return fail("a whole length changed by round");
    /* a KallistiOS texture pointer (0xa5xxxxxx) in the two forms the console ran with */
    if (sh4der_pvrx_dma_src(0xa5123460u, SH4DER_PVRX_AREA_64) != 0x04123460u) return fail("64-bit area word");
    if (sh4der_pvrx_dma_src(0xa5123460u, SH4DER_PVRX_AREA_32) != 0x05123460u) return fail("32-bit area word");
    if (sh4der_pvrx_dma_src(0x00fffffeu, 7) != 0x04fffffeu) return fail("an unknown area is not the 64-bit one");
    /* a RAM pointer (0x8cxxxxxx): segment bits and the low five bits off */
    if (sh4der_pvrx_dma_dst(0x8c0456e0u) != 0x0c0456e0u) return fail("RAM word");
    if (sh4der_pvrx_dma_dst(0x8c0456ffu) != 0x0c0456e0u) return fail("RAM word keeps low bits");
    return 0;
}

static int ta_counts(void) {
    sh4der_pvrx_ta_t base, now, d;
    unsigned i;

    memset(&base, 0, sizeof base); memset(&now, 0, sizeof now);
    sh4der_pvrx_ta_since(&now, &base, &d);
    if (sh4der_pvrx_ta_overflow(&d) || sh4der_pvrx_ta_any(&d)) return fail("no events, a flag set");
    for (i = 0; i < SH4DER_PVRX_TA_EVENTS; i++) {
        const int want = i == SH4DER_PVRX_TA_ISP || i == SH4DER_PVRX_TA_PARAM || i == SH4DER_PVRX_TA_OPB || i == SH4DER_PVRX_TA_INPUT_OVERFLOW;

        memset(&base, 0, sizeof base); memset(&now, 0, sizeof now);
        base.n[i] = 0xfffffffeu; now.n[i] = 3u;                  /* five events across the wrap */
        sh4der_pvrx_ta_since(&now, &base, &d);
        if (d.n[i] != 5u) return fail("difference across a wrap");
        if (!sh4der_pvrx_ta_any(&d)) return fail("an event, the any flag clear");
        if (sh4der_pvrx_ta_overflow(&d) != want) return fail("overflow flag for the wrong events");
        base = now;                                              /* the next frame: quiet again */
        sh4der_pvrx_ta_since(&now, &base, &d);
        if (sh4der_pvrx_ta_any(&d)) return fail("a quiet frame after a base, a flag set");
    }
    if (SH4DER_PVRX_TA_EVENTS != 6) return fail("six events, 0x0200 to 0x0205");
    return 0;
}

static int revision(void) {
    if (strcmp(sh4der_pvrx_revision_name(0x0011u, 0x0bu), "2.42")) return fail("our console's reads are not 2.42");
    if (strcmp(sh4der_pvrx_revision_name(0x17fd0011u, 0xffffff0bu), "2.42")) return fail("the high bits count");
    if (strcmp(sh4der_pvrx_revision_name(0x0001u, 0x02u), "1.5")) return fail("1.5");
    if (strcmp(sh4der_pvrx_revision_name(0x0011u, 0x08u), "2.2")) return fail("2.2");
    if (strcmp(sh4der_pvrx_revision_name(0x0011u, 0x09u), "2.3")) return fail("2.3");
    if (strcmp(sh4der_pvrx_revision_name(0x0011u, 0x0au), "2.4 or 2.41")) return fail("2.4");
    if (strcmp(sh4der_pvrx_revision_name(0x0011u, 0x0cu), "unknown")) return fail("an unlisted bus revision named");
    if (strcmp(sh4der_pvrx_revision_name(0x0001u, 0x0bu), "unknown")) return fail("a 1.5 core with a 2.42 bus named");
    if (strcmp(sh4der_pvrx_revision_name(0x0012u, 0x0bu), "unknown")) return fail("an unlisted core named");
    return 0;
}

static int clip(void) {
    uint32_t cmd[8];
    unsigned t[4];
    float v[3][3];
    unsigned i;

    /* the two halves of a 640 x 480 screen, as the spike cut them: tiles 0..9 and 10..19, rows 0..14 */
    if (!sh4der_pvrx_clip_tiles(0u, 0u, 320u, 480u, t) || t[0] != 0u || t[1] != 0u || t[2] != 9u || t[3] != 14u) return fail("left half tiles");
    if (!sh4der_pvrx_clip_tiles(320u, 0u, 640u, 480u, t) || t[0] != 10u || t[1] != 0u || t[2] != 19u || t[3] != 14u) return fail("right half tiles");
    if (!sh4der_pvrx_clip_tiles(33u, 31u, 34u, 33u, t) || t[0] != 1u || t[1] != 0u || t[2] != 1u || t[3] != 1u) return fail("a small rectangle's tiles");
    if (sh4der_pvrx_clip_tiles(10u, 0u, 10u, 5u, t) || sh4der_pvrx_clip_tiles(0u, 9u, 5u, 3u, t)) return fail("an empty rectangle has tiles");

    memset(cmd, 0xee, sizeof cmd);
    if (!sh4der_pvrx_clip_cmd(cmd, 10u, 0u, 19u, 14u, 20u, 15u)) return fail("the right half refused");
    if (cmd[0] != 0x20000000u || cmd[1] || cmd[2] || cmd[3] || cmd[4] != 10u || cmd[5] != 0u || cmd[6] != 19u || cmd[7] != 14u) return fail("clip words");
    memset(cmd, 0xee, sizeof cmd);
    if (sh4der_pvrx_clip_cmd(cmd, 0u, 0u, 20u, 14u, 20u, 15u)) return fail("a clip past the last column taken");
    if (sh4der_pvrx_clip_cmd(cmd, 0u, 0u, 19u, 15u, 20u, 15u)) return fail("a clip past the last row taken");
    if (sh4der_pvrx_clip_cmd(cmd, 5u, 0u, 4u, 14u, 20u, 15u)) return fail("a clip turned round in x taken");
    if (sh4der_pvrx_clip_cmd(cmd, 0u, 7u, 19u, 6u, 20u, 15u)) return fail("a clip turned round in y taken");
    for (i = 0; i < 8u; i++) if (cmd[i] != 0xeeeeeeeeu) return fail("a refused clip wrote its words");
    if (!sh4der_pvrx_clip_cmd(cmd, 3u, 4u, 3u, 4u, 20u, 15u)) return fail("a one-tile clip refused");

    if (!sh4der_pvrx_clip_dummy(v)) return fail("the dummy triangle has no area");
    for (i = 0; i < 3u; i++) {
        if (!(v[i][0] < 0.0f)) return fail("the dummy triangle reaches the screen");
        if (!(v[i][2] > 0.0f)) return fail("the dummy triangle's 1 / w is not positive");
    }
    {   /* twice its area: 32 x 32 pixels */
        const float a2 = fabsf((v[1][0] - v[0][0]) * (v[2][1] - v[0][1]) - (v[1][1] - v[0][1]) * (v[2][0] - v[0][0]));

        if (fabsf(a2 - 1024.0f) > 0.01f) return fail("the dummy triangle is not the spike's");
    }
    return 0;
}

int main(void) {
    int bad = 0;

    bad += wait_count();
    bad += dma_words();
    bad += ta_counts();
    bad += revision();
    bad += clip();
    if (bad) return 1;
    printf("pvrx_test OK\n");
    return 0;
}
