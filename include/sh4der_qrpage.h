/* SH4DER-PVR: result pages as QR codes, the test set (image psm-qr-q1)

   include/sh4der_qrpage.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (console: VID-014 in wiki/hardware/ledger-video.md; wiki/effects/qrpage.md)
   Cost:       one page drawn once. The encoder runs once a code (its time is not measured on the console). RAM: the panel and the
               modules are drawn straight into the frame buffer; the code buffer is QR_BUF bytes (version 5).
   Start with: the set's own table (qr_sets): 16 codes at 10, 8, 6 and 4 px; panel level 232 of 255.
   Status:     MEASURED, one run (VID-014): the 16 codes read from one phone photo, each to its exact payload with its CRC right
               (the reader was OpenCV's detector on the photo).
   Do:         send every code as its own payload string (q1 <id> <crc>|<data>): a decoded string checks itself.
   Do not:     use it for a number that is not already on a text page; the codes repeat the text, they do not replace it.
   Maturity:   ALPHA v0.5 (the page kit's row in wiki/status/maturity.md)
   Needs:      Project Nayuki's qrcodegen.c and qrcodegen.h (MIT), included before this header. Where they come from: see
               "THE ENCODER IS NOT OURS" below. Include sh4der_page.h first (the page kit's text and rectangles).

   The names are the QR set's own (qr_*, QR_*). demo/psm/qr/qr_page.h is a forward at the old path.

   A page of a console test is photographed off a television and typed by hand. A QR code on the page would carry the
   same bytes with no typing. Whether a phone reads a code off this picture, and at which size, is not known: this file
   is the test set for that. It draws known payloads at several module sizes; the owner points a phone at each page.

   THE ENCODER IS NOT OURS. The codes are made by Project Nayuki's QR Code generator library (C), MIT License,
   https://www.nayuki.io/page/qr-code-generator-library (qrcodegen.c, qrcodegen.h). It is used as a library and is not
   in this repository: demo/psm/tools/fetch.py downloads the two files named in demo/psm/fetch_table.json into demo/psm/assets/_dl and
   checks their sha256; demo/psm/qr/build.py compiles them from there. Its notice stays in its files; demo/psm/CREDITS.md and the
   README's "Credits and license" have its row. A client gets the same two files from the same URLs (fetch_table.json), or any
   copy of them, and includes qrcodegen.h before this header.
   Nothing of it is copied into this file: only its public calls are used (qrcodegen_encodeBinary, _getSize, _getModule).

   THE SET. Every code is error correction level M, byte mode, filled to the version's capacity: version 3 (29 x 29
   modules) holds 42 bytes, version 5 (37 x 37) holds 84. A code's payload is text a phone shows as it is:

       q1 <id> <crc>|<data>

   id is version-pixels-letter ("3-08-a": version 3, 8 pixels a module, the first such code); data is 23 or 65
   letters and digits from a fixed generator seeded by the id (qr_fill); crc is the CRC-32 (zip's) of data as 8 hex
   digits. So a decoded string checks itself, and tools/qr_check.py also makes every payload again from its id.

   THE PAGES (qr_sets). One set a page, each code dark on a light panel with the 4-module quiet zone the standard
   asks for, all inside the title-safe area (x 64 .. 575, y 48 .. 431):
       version 3 at 10 px   1 code      version 3 at 8 px   1 code      version 5 at 8 px   1 code
       version 3 at 6 px    2 codes     version 5 at 6 px   1 code
       version 3 at 4 px    6 codes     version 5 at 4 px   4 codes
   A page with room at the right of its codes has its text there; the others have a header line above and the ids
   below. The panel's level is the caller's (232 of 255 at the start; 255 may bloom on a tube).

   Include qrcodegen.h first, then sh4der_page.h (pg_text, pg_rect, pg_crc, the frame buffer), then this header.
*/

#ifndef SH4DER_QRPAGE_H
#define SH4DER_QRPAGE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SH4DER_PAGE_H
#error "sh4der_qrpage.h: include qrcodegen.h, then sh4der_page.h, before this header"
#endif

#define QR_QUIET 4u                         /* modules of panel round a code */
#define QR_BUF qrcodegen_BUFFER_LEN_FOR_VERSION(5)
#define QR_PAYLOAD_MAX 84u

typedef struct qr_set { uint8_t ver, px, count, cols; } qr_set_t;

static const qr_set_t qr_sets[] = { { 3, 10, 1, 1 }, { 3, 8, 1, 1 }, { 5, 8, 1, 1 }, { 3, 6, 2, 2 }, { 5, 6, 1, 1 }, { 3, 4, 6, 3 }, { 5, 4, 4, 2 } };
#define QR_SETS (sizeof qr_sets / sizeof qr_sets[0])

static inline unsigned qr_capacity(unsigned ver) { return ver == 3u ? 42u : 84u; }      /* bytes, level M, byte mode */
static inline unsigned qr_modules(unsigned ver) { return 17u + 4u * ver; }

/* The payload of code k of a set: capacity characters and a 0. */
static void qr_fill(const qr_set_t *s, unsigned k, char out[QR_PAYLOAD_MAX + 1u]) {
    static const char alnum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const unsigned n = qr_capacity(s->ver) - 19u;
    char id[7], data[QR_PAYLOAD_MAX + 1u], crc[12];
    uint32_t r;
    unsigned i;

    id[0] = (char)('0' + s->ver % 10u); id[1] = '-'; id[2] = (char)('0' + s->px / 10u % 10u); id[3] = (char)('0' + s->px % 10u); id[4] = '-';
    id[5] = (char)('a' + k % 26u); id[6] = '\0';
    r = pg_crc(0, id, 6u);
    for (i = 0; i < n; i++) { r = r * 1103515245u + 12345u; data[i] = alnum[(r >> 16) % 62u]; }
    snprintf(crc, sizeof crc, "%08lx", (unsigned long)pg_crc(0, data, n) & 0xfffffffful);
    memcpy(out, "q1 ", 3u); memcpy(out + 3, id, 6u); out[9] = ' '; memcpy(out + 10, crc, 8u); out[18] = '|';
    memcpy(out + 19, data, n); out[19u + n] = '\0';
}

/* The code of a payload, in the encoder's own form. 0 when the encoder refuses. */
static int qr_encode(const qr_set_t *s, const char *payload, uint8_t code[QR_BUF]) {
    uint8_t temp[QR_BUF];
    const size_t n = strlen(payload);

    memcpy(temp, payload, n);
    return qrcodegen_encodeBinary(temp, n, code, qrcodegen_Ecc_MEDIUM, s->ver, s->ver, qrcodegen_Mask_AUTO, false) ? 1 : 0;
}

/* Where a set's codes are: the panel of code k starts at (x, y) and is side pixels square; the modules start
   QR_QUIET modules in. side_text: 1 when the page's text is at the right of the codes, 0 when above and below. */
static inline unsigned qr_side(const qr_set_t *s) { return (qr_modules(s->ver) + 2u * QR_QUIET) * s->px; }
static inline unsigned qr_rows(const qr_set_t *s) { return (s->count + s->cols - 1u) / s->cols; }
static inline int qr_side_text(const qr_set_t *s) { return PG_X0 + qr_side(s) * s->cols + 8u + 16u * PG_CW <= 640u - PG_X0; }
static inline unsigned qr_x(const qr_set_t *s, unsigned k) { return PG_X0 + (k % s->cols) * qr_side(s); }
static inline unsigned qr_y(const qr_set_t *s, unsigned k) { return PG_Y0 + (qr_side_text(s) ? 0u : PG_CH + 4u) + (k / s->cols) * qr_side(s); }

/* A set as a page: `page` of `pages` is for the header; white is the panel's level, 0 .. 255. 0 when a code failed. */
static int qr_draw(const qr_set_t *s, unsigned page, unsigned pages, unsigned white) {
    const uint16_t panel = PG_RGB(white, white, white);
    const unsigned m = qr_modules(s->ver), side = qr_side(s);
    char payload[QR_PAYLOAD_MAX + 1u], line[PG_COLS + 1u];
    uint8_t code[QR_BUF];
    unsigned k, i, j, ok = 1u;

    vid_clear(0, 0, 0);
    if (qr_side_text(s)) {
        const unsigned tx = PG_X0 + side * s->cols + 8u;
        unsigned ty = PG_Y0;

        snprintf(line, sizeof line, "QR q1 p%u/%u", page + 1u, pages); pg_text(tx, ty, PG_WHITE, line); ty += PG_CH;
        snprintf(line, sizeof line, "V%u-M %u px x%u", (unsigned)s->ver, (unsigned)s->px, (unsigned)s->count); pg_text(tx, ty, PG_WHITE, line); ty += PG_CH;
        snprintf(line, sizeof line, "%u bytes a code", qr_capacity(s->ver)); pg_text(tx, ty, PG_GREY, line); ty += PG_CH;
        snprintf(line, sizeof line, "panel %u of 255", white); pg_text(tx, ty, PG_GREY, line); ty += 2u * PG_CH;
        pg_text(tx, ty, PG_GREY, "id and CRC:"); ty += PG_CH;
        for (k = 0; k < s->count; k++) {
            qr_fill(s, k, payload);
            snprintf(line, sizeof line, "%.6s %.8s", payload + 3, payload + 10); pg_text(tx, ty, PG_WHITE, line); ty += PG_CH;
        }
        ty += PG_CH;
        pg_text(tx, ty, PG_GREY, "A/B page"); ty += PG_CH;
        pg_text(tx, ty, PG_GREY, "Y panel level");
    } else {
        unsigned ty = qr_y(s, 0) + qr_rows(s) * side + 4u;

        snprintf(line, sizeof line, "QR q1 p%u/%u V%u-M %u px x%u, %u bytes a code, panel %u A/B Y", page + 1u, pages, (unsigned)s->ver, (unsigned)s->px,
                 (unsigned)s->count, qr_capacity(s->ver), white);
        pg_text(PG_X0, PG_Y0, PG_WHITE, line);
        for (k = 0; k < s->count; k++) {                   /* the ids and CRCs: three a line, in the codes' order */
            qr_fill(s, k, payload);
            snprintf(line, sizeof line, "%.6s %.8s", payload + 3, payload + 10);
            pg_text(PG_X0 + (k % 3u) * 20u * PG_CW, ty + (k / 3u) * PG_CH, PG_WHITE, line);
        }
    }
    for (k = 0; k < s->count; k++) {
        const unsigned x0 = qr_x(s, k), y0 = qr_y(s, k);

        qr_fill(s, k, payload);
        pg_rect((int)x0, (int)y0, (int)(x0 + side - 1u), (int)(y0 + side - 1u), panel);
        if (!qr_encode(s, payload, code) || (unsigned)qrcodegen_getSize(code) != m) { ok = 0u; continue; }
        for (j = 0; j < m; j++)
            for (i = 0; i < m; i++)
                if (qrcodegen_getModule(code, (int)i, (int)j)) {
                    const unsigned x = x0 + (QR_QUIET + i) * s->px, y = y0 + (QR_QUIET + j) * s->px;

                    pg_rect((int)x, (int)y, (int)(x + s->px - 1u), (int)(y + s->px - 1u), 0u);
                }
    }
    return (int)ok;
}

#endif /* SH4DER_QRPAGE_H */
