/* SH4DER-PVR: the QR test set's host test

   demo/psm/qr/test/qr_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   ../qr_page.h on the host, with stand-ins for the three KallistiOS names ../../psm_page.h uses (the frame buffer,
   vid_clear, bfont_draw; the stand-in font is boxes, as in ../../compare/test/psm_page_test.c). Checks: every payload
   has its version's full length, its prefix and its own CRC; the encoder takes each at the fixed version; a drawn
   page has every module's pixels as the encoder's module says, the quiet zone clear, and nothing outside the
   title-safe area. Writes, into the folder given:
     qr_matrices.txt   every code as its payload and its modules, for ../../tools/qr_check.py --matrices (a decoder
                       that shares no code with the encoder)
     qr_p<n>.png       the pages of codes, for an image decoder and to look at

   Build (from the repository's root; the encoder is fetched by demo/psm/tools/fetch.py, see ../build.py):
     gcc -std=c99 -O2 -Wall -Wextra -Idemo/psm/assets/_dl demo/psm/qr/test/qr_test.c demo/psm/assets/_dl/qrcodegen.c -o build/qr_test
     build/qr_test <folder>
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qrcodegen.h"

/* ---- stand-ins for KallistiOS ---- */
#define GUARD 640u * 8u
static uint16_t fb[GUARD + 640u * 480u + GUARD];
#define vram_s (fb + GUARD)
static void vid_clear(int r, int g, int b) { (void)r; (void)g; (void)b; memset(vram_s, 0, 640u * 480u * 2u); }

static size_t bfont_draw(void *buffer, uint32_t bufwidth, bool opaque, uint32_t c) {
    uint16_t *b = buffer;
    unsigned x, y;

    (void)opaque;
    if (c != ' ') for (y = 6u; y < 20u; y++) for (x = 2u; x < 9u; x++) if (y == 6u || y == 19u || x == 2u || x == 8u) b[y * bufwidth + x] = 0xffffu;
    return 12u;
}

#define PG_PREFIX "test"
#include "../../psm_page.h"
#include "../qr_page.h"

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
    if (!f) { printf("cannot write %s\n", path); fails++; return; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, 640u); be32(ihdr + 4, 480u); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13u);
    chunk(f, "IDAT", z, (uint32_t)(q - z));
    chunk(f, "IEND", z, 0u);
    fclose(f);
    printf("  wrote %s\n", path);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    char payload[QR_PAYLOAD_MAX + 1u], path[512], name[32];
    uint8_t code[QR_BUF];
    unsigned s, k, i, j, x, y, n, codes = 0, v;
    FILE *mf;

    (void)pg_say; (void)pg_plot; (void)pg_series; (void)pg_point; (void)pg_draw;      /* parts of the page kit this test does not use */
    for (n = 0; n < 256u; n++) { uint32_t c = n; for (k = 0; k < 8u; k++) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1; crc_table[n] = c; }
    CHECK(pg_crc(0, "123456789", 9u) == 0xcbf43926u);
    snprintf(path, sizeof path, "%s/qr_matrices.txt", dir);
    mf = fopen(path, "w");
    if (!mf) { printf("cannot write %s\n", path); return 1; }

    for (s = 0; s < QR_SETS; s++) {
        const qr_set_t *q = &qr_sets[s];
        const unsigned m = qr_modules(q->ver), side = qr_side(q), cap = qr_capacity(q->ver);

        CHECK(q->count <= 26u && q->cols >= 1u);
        for (v = 0; v < 3u; v++) {                          /* the three panel levels */
            static const unsigned level[3] = { 232u, 255u, 176u };
            const uint16_t panel = PG_RGB(level[v], level[v], level[v]);

            memset(fb, 0, sizeof fb);
            CHECK(qr_draw(q, s + 2u, QR_SETS + 2u, level[v]));
            for (i = 0; i < GUARD; i++) CHECK(fb[i] == 0u && fb[GUARD + 640u * 480u + i] == 0u);
            for (y = 0; y < 480u; y++)
                for (x = 0; x < 640u; x++)
                    if (vram_s[y * 640u + x] && (x < 64u || x > 575u || y < 48u || y > 431u)) { CHECK(!"a pixel outside the title-safe area"); x = 640u; y = 480u; }
            for (k = 0; k < q->count; k++) {
                const unsigned x0 = qr_x(q, k), y0 = qr_y(q, k);
                unsigned wrong = 0;

                qr_fill(q, k, payload);
                CHECK(qr_encode(q, payload, code));
                CHECK((unsigned)qrcodegen_getSize(code) == m);
                for (y = 0; y < side; y++)
                    for (x = 0; x < side; x++) {
                        const int mi = (int)(x / q->px) - (int)QR_QUIET, mj = (int)(y / q->px) - (int)QR_QUIET;
                        const int dark = mi >= 0 && mj >= 0 && mi < (int)m && mj < (int)m && qrcodegen_getModule(code, mi, mj);

                        wrong += vram_s[(y0 + y) * 640u + x0 + x] != (dark ? 0u : panel);
                    }
                CHECK(wrong == 0u);
            }
        }
        for (k = 0; k < q->count; k++) {                    /* the payloads and the modules, once a code */
            qr_fill(q, k, payload);
            CHECK(strlen(payload) == cap);
            CHECK(!memcmp(payload, "q1 ", 3u) && payload[9] == ' ' && payload[18] == '|');
            {
                char want[12];

                snprintf(want, sizeof want, "%08lx", (unsigned long)pg_crc(0, payload + 19, cap - 19u));
                CHECK(!memcmp(want, payload + 10, 8u));
            }
            CHECK(qr_encode(q, payload, code));
            fprintf(mf, "CODE %u %u %s\n", (unsigned)q->ver, m, payload);
            for (j = 0; j < m; j++) { for (i = 0; i < m; i++) fputc(qrcodegen_getModule(code, (int)i, (int)j) ? '1' : '0', mf); fputc('\n', mf); }
            codes++;
        }
        qr_draw(q, s + 2u, QR_SETS + 2u, 232u);
        snprintf(name, sizeof name, "qr_p%u.png", s + 3u);
        png(dir, name);
    }
    fclose(mf);
    CHECK(codes == 16u);
    printf("  wrote %s (%u codes)\n", path, codes);
    printf("qr_test: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
