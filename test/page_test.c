/* SH4DER-PVR: the result pages' host test (layout and arithmetic; not the console's font)

   test/page_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   include/sh4der_page.h on the host, with stand-ins for the three KallistiOS names it uses (the frame buffer, vid_clear,
   bfont_draw). The BIOS font is not on the host: the stand-in draws digits as 5 x 7 cells made 2 x 3 larger and every
   other character as a small box, which is enough to check the layout and the reduction to 8 x 16, and NOT enough to
   judge how the real font photographs. Checks: the number and tick arithmetic, the wrap, the page count, that nothing
   is drawn outside the safe area or the frame buffer, that a stroke one pixel wide keeps a bright pixel in every row.
   Writes a text page and two graph pages as PNG files (stored, no compression) to look at.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra test/page_test.c -o build/page_test
     build/page_test [folder for the PNG files]   (make test runs it with no folder: no PNG files)
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- stand-ins for KallistiOS ---- */
#define GUARD 640u * 8u
static uint16_t fb[GUARD + 640u * 480u + GUARD];
#define vram_s (fb + GUARD)
static void vid_clear(int r, int g, int b) { (void)r; (void)g; (void)b; memset(vram_s, 0, 640u * 480u * 2u); }

static const uint8_t digit57[10][7] = {
    { 14, 17, 19, 21, 25, 17, 14 }, { 4, 12, 4, 4, 4, 4, 14 }, { 14, 17, 1, 2, 4, 8, 31 }, { 31, 2, 4, 2, 1, 17, 14 }, { 2, 6, 10, 18, 31, 2, 2 },
    { 31, 16, 30, 1, 1, 17, 14 }, { 6, 8, 16, 30, 17, 17, 14 }, { 31, 1, 2, 4, 8, 8, 8 }, { 14, 17, 17, 14, 17, 17, 14 }, { 14, 17, 17, 15, 1, 2, 12 } };

static size_t bfont_draw(void *buffer, uint32_t bufwidth, bool opaque, uint32_t c) {
    uint16_t *b = buffer;
    unsigned x, y;

    (void)opaque;
    if (c >= '0' && c <= '9') {
        for (y = 0; y < 21u; y++) for (x = 0; x < 10u; x++) if (digit57[c - '0'][y / 3u] & (16u >> (x / 2u))) b[(y + 2u) * bufwidth + x + 1u] = 0xffffu;
    } else if (c == '|') {
        for (y = 0; y < 24u; y++) b[y * bufwidth + 4u] = 0xffffu;         /* a stroke one pixel wide, on a column the filter splits */
    } else if (c != ' ') {
        for (y = 6u; y < 20u; y++) for (x = 2u; x < 9u; x++) if (y == 6u || y == 19u || x == 2u || x == 8u) b[y * bufwidth + x] = 0xffffu;
    }
    return 12u;
}

#define PG_PREFIX "test"
#include "../include/sh4der_page.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* ---- a PNG with stored (uncompressed) data ---- */
static uint32_t crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n) { while (n--) c = crc_table[(c ^ *p++) & 255u] ^ (c >> 8); return c; }
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n) {
    uint8_t h[8], t[4];
    uint32_t c;

    be32(h, n); memcpy(h + 4, type, 4u);
    c = crc(0xffffffffu, h + 4, 4u); c = crc(c, data, n) ^ 0xffffffffu;
    be32(t, c);
    fwrite(h, 1, 8, f); fwrite(data, 1, n, f); fwrite(t, 1, 4, f);
}

static void png(const char *dir, const char *name) {
    static uint8_t raw[480u * (1u + 640u * 3u)], z[sizeof raw + 480u * 5u + 16u];
    const uint32_t row = 1u + 640u * 3u;
    uint8_t ihdr[13], *q = z;
    uint32_t a = 1u, b = 0u, x, y, i;
    char path[512];
    FILE *f;

    for (y = 0; y < 480u; y++) {
        raw[y * row] = 0u;
        for (x = 0; x < 640u; x++) {
            const uint16_t p = vram_s[y * 640u + x];
            uint8_t *o = &raw[y * row + 1u + x * 3u];

            o[0] = (uint8_t)((p >> 11) * 255u / 31u); o[1] = (uint8_t)(((p >> 5) & 63u) * 255u / 63u); o[2] = (uint8_t)((p & 31u) * 255u / 31u);
        }
    }
    *q++ = 0x78; *q++ = 0x01;
    for (y = 0; y < 480u; y++) {
        *q++ = (uint8_t)(y == 479u); *q++ = (uint8_t)row; *q++ = (uint8_t)(row >> 8); *q++ = (uint8_t)~row; *q++ = (uint8_t)(~row >> 8);
        memcpy(q, &raw[y * row], row); q += row;
    }
    for (i = 0; i < sizeof raw; i++) { a = (a + raw[i]) % 65521u; b = (b + a) % 65521u; }
    be32(q, (b << 16) | a); q += 4;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); return; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, 640u); be32(ihdr + 4, 480u); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13u);
    chunk(f, "IDAT", z, (uint32_t)(q - z));
    chunk(f, "IEND", z, 0u);
    fclose(f);
    printf("  wrote %s\n", path);
}

/* Nothing outside the frame buffer, and nothing outside the title-safe area x 64 .. 575, y 48 .. 431. */
static void bounds(const char *what) {
    unsigned i, x, y, out = 0;

    for (i = 0; i < GUARD; i++) out += fb[i] != 0u || fb[GUARD + 640u * 480u + i] != 0u;
    CHECK(out == 0u);
    for (y = 0; y < 480u; y++) for (x = 0; x < 640u; x++) if ((x < 64u || x > 575u || y < 48u || y > 431u) && vram_s[y * 640u + x]) out++;
    if (out) printf("  %s: %u pixels outside the safe area\n", what, out);
    CHECK(out == 0u);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : NULL;
    char s[24];
    unsigned i, k, y, x;
    pg_plot_t *p;

    for (i = 0; i < 256u; i++) { uint32_t c = i; for (k = 0; k < 8u; k++) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1; crc_table[i] = c; }

    pg_num(s, sizeof s, 0.0f); CHECK(!strcmp(s, "0"));
    pg_num(s, sizeof s, 12.0f); CHECK(!strcmp(s, "12"));
    pg_num(s, sizeof s, 0.5f); CHECK(!strcmp(s, "0.5"));
    pg_num(s, sizeof s, 2.25f); CHECK(!strcmp(s, "2.25"));
    pg_num(s, sizeof s, 39928.4f); CHECK(!strcmp(s, "39928"));
    pg_num(s, sizeof s, -4.0f); CHECK(!strcmp(s, "-4"));
    pg_num(s, sizeof s, 0.999f); CHECK(!strcmp(s, "1"));
    CHECK(pg_round_up(0.0f) == 1.0f && pg_round_up(1.0f) == 1.0f && pg_round_up(1.2f) == 2.0f && pg_round_up(4.9f) == 5.0f && pg_round_up(7.0f) == 10.0f);
    CHECK(pg_round_up(39928.0f) == 50000.0f && pg_round_up(0.03f) > 0.0499f && pg_round_up(0.03f) < 0.0501f && pg_round_up(1262.0f) == 2000.0f);

    pg_init();
    for (y = 0; y < PG_CH; y++) {          /* the one-pixel stroke: a pixel of at least 150 of 255 in every row */
        unsigned top = 0;

        for (x = 0; x < PG_CW; x++) if (pg_glyph['|' - 32][y][x] > top) top = pg_glyph['|' - 32][y][x];
        CHECK(top >= 150u);
    }
    for (x = 0; x < PG_CW; x++) CHECK(pg_glyph[' ' - 32][3][x] == 0u);

    for (i = 0; i < 30u; i++) pg_say("%u RESULT line %u: 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789", i % 10u, i);
    CHECK(pg_nlines == 60u);               /* 90 characters and more: each line wraps once */
    CHECK(strlen(pg_lines[0]) <= PG_COLS && !strncmp(pg_lines[1], "   0123456789", 13u));   /* cut at a space: no number split */
    CHECK(pg_lines[0][strlen(pg_lines[0]) - 1u] == '9');
    { static char longword[100]; unsigned n0 = pg_nlines; memset(longword, 'x', 99u); pg_say("%s", longword); CHECK(pg_nlines == n0 + 2u && strlen(pg_lines[n0]) == PG_COLS); pg_nlines = n0; }
    pg_break();
    {   /* a cut where the text has two spaces: the first stays at the line's end, and the CRC counts it. tools/page_crc.py
           has the same line and the same value in its self-test: the console is the reference (found on the first suite run) */
        const unsigned n0 = pg_nlines;
        uint32_t c = 0;

        pg_say("  used KB 65/768/1024  est KB 754/754  KOS max 1024K  SHED<=0  text 92v");
        CHECK(pg_nlines == n0 + 2u);
        CHECK(!strcmp(pg_lines[n0], "  used KB 65/768/1024  est KB 754/754  KOS max 1024K  SHED<=0 "));
        CHECK(!strcmp(pg_lines[n0 + 1u], "   text 92v"));
        c = pg_crc(c, pg_lines[n0], strlen(pg_lines[n0])); c = pg_crc(c, "\n", 1u);
        c = pg_crc(c, pg_lines[n0 + 1u], strlen(pg_lines[n0 + 1u])); c = pg_crc(c, "\n", 1u);
        CHECK(c == 0x10badf18u);
        pg_nlines = n0;
    }
    CHECK(pg_nlines == 66u && pg_text_pages() == 3u);
    pg_say("8 FRAME 60 frames, bands 2 wide; us");
    CHECK(pg_text_pages() == 4u);
    CHECK(pg_crc(0u, "123456789", 9u) == 0xcbf43926u);           /* the CRC-32 check value */
    CHECK(pg_crc(pg_crc(0u, "1234", 4u), "56789", 5u) == 0xcbf43926u);
    CHECK(pg_page_crc(3u) == pg_crc(0u, "8 FRAME 60 frames, bands 2 wide; us\n", 36u));
    CHECK(pg_page_crc(0u) != pg_page_crc(1u));
    printf("  page 4's CRC %08lx (tools/page_crc.py must give the same for its one line)\n", (unsigned long)pg_page_crc(3u));

    p = pg_plot("8 FRAME: each frame's time, us (the exact values are on the text pages)", "frame", "us", 0);
    pg_series(p, "KallistiOS waits"); for (i = 0; i < 60u; i++) pg_point(p, (float)(i + 1u), (i % 5u) == 3u ? 29947.0f : 39928.0f);
    pg_series(p, "ready polled"); for (i = 0; i < 60u; i++) pg_point(p, (float)(i + 1u), (i % 7u) == 2u ? 29946.0f : 39930.0f);
    pg_series(p, "8b: render-done polled too"); for (i = 0; i < 60u; i++) pg_point(p, (float)(i + 1u), 33366.0f);
    p = pg_plot("8 FRAME: the done-wait of each pass, mean us", "pass", "us", 1);
    pg_cat(p, "map"); pg_cat(p, "mask"); pg_cat(p, "screen");
    pg_series(p, "KallistiOS waits"); pg_point(p, 0.0f, 8074.0f); pg_point(p, 1.0f, 9102.0f); pg_point(p, 2.0f, 19011.0f);
    pg_series(p, "ready polled"); pg_point(p, 0.0f, 8031.0f); pg_point(p, 1.0f, 9064.0f); pg_point(p, 2.0f, 19262.0f);
    pg_series(p, "8b"); pg_point(p, 0.0f, 3400.0f); pg_point(p, 1.0f, 6200.0f); pg_point(p, 2.0f, 16000.0f);
    pg_series(p, "the PVR's render"); pg_point(p, 0.0f, 3341.0f); pg_point(p, 1.0f, 6181.0f); pg_point(p, 2.0f, 9904.0f);
    CHECK(pg_pages() == 6u);

    pg_draw(0u, "PVR CHAIN c4", "START live Y tiles"); bounds("text page");
    if (dir) png(dir, "psm_page_text.png");
    { unsigned lit = 0; for (i = 0; i < 640u * 480u; i++) lit += vram_s[i] != 0u; CHECK(lit > 20000u); }
    pg_draw(4u, "PVR CHAIN c4", "START live Y tiles"); bounds("line graph");
    if (dir) png(dir, "psm_page_lines.png");
    pg_draw(5u, "PVR CHAIN c4", "START live Y tiles"); bounds("bar graph");
    if (dir) png(dir, "psm_page_bars.png");

    for (i = 0; i < 40u; i++) p = pg_plot("one too many", "x", "y", 0);   /* more graphs than there is room for: no overrun */
    CHECK(pg_nplots == PG_PLOTS && p == &pg_spare);
    for (i = 0; i < 200u; i++) pg_point(p, 1.0f, 1.0f);
    pg_series(p, "s"); for (i = 0; i < 200u; i++) pg_point(p, 1.0f, 1.0f);
    CHECK(p->n[0] == PG_POINTS);
    bounds("after the overflow");

    if (fails) printf("page_test: %d FAILED\n", fails); else printf("page_test: ok (%u x %u characters a page)\n", PG_COLS, PG_ROWS);
    return fails != 0;
}
