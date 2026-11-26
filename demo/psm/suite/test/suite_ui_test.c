/* SH4DER-PVR: host test of the suite menu's panels (../suite_ui.h) and its skin (../suite_skin.h)

   demo/psm/suite/test/suite_ui_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Draws the menu's screens into a buffer of its own and checks them: the text inside the title-safe area, the panels
   and chips inside the action-safe area, a panel under every option and a line of background between two, a chip
   under every key of the legend, each text colour readable on what it is drawn over (contrast 4.5 : 1 or more), the
   9-slice drawn at its own size giving the skin back pixel for pixel, a slot too small for its 9-slice drawn plain,
   and the text blended over what is under it. The glyphs here are blocks (every pixel of a character's cell but its
   last column and row), so a text pixel is one of the text's colours exactly.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra -Werror demo/psm/suite/test/suite_ui_test.c -o build/psm_suite/suite_ui_test
   -ppm <dir> also writes each screen as a PPM. With -DUI_MINIFONT -I<KallistiOS>/kernel/arch/dreamcast/util the
   glyphs are KallistiOS's minifont, to look at; the checks of text then do not run (the console's font is the BIOS
   font through the page kit, not this one). */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../suite_skin.h"
#include "../../psm_credits.h"

#ifdef UI_MINIFONT
#include "minifont.h"
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint16_t fb[640 * 480];
static uint8_t glyph[96][16][8];
static const ui_font_t font = { &glyph[0][0][0], 8u, 16u };
static const char *ppm_dir;

/* s7's list, with a plain title and a shows line on some entries (the JSON's "title" and "shows"). */
static const suite_entry_t list[] = {
    { "AO4.BIN", "ao4", "AO side by side; clip guard; tile cut last. 110 s", "photo pages+graph", 1u, 1u, 600000u, NAV_DEMOS,
      "Room shading, four ways", "corner shading: none, baked, ground, texture" },
    { "AOLAMP.BIN", "ao-lamp", "lamp look, movable split", "LT/RT move the split", 2u, 1u, 600000u, NAV_DEMOS,
      "Lamp with a lit shade", "light through a thin shade, glow halo" },
    { "HERO1.BIN", "hero1", "skinned hero, pass costs, room + hens. ~50 s", "photo every page", 3u, 1u, 600000u, NAV_DEMOS, 0, 0 },
    { "PL4NAR.BIN", "pl4nar", "street demo, new map", "photo the pages", 4u, 1u, 900000u, NAV_DEMOS,
      "Car in a city by day", "reflection, car shadow, corner shading, heat haze" },
    { "D1.BIN", "d1", "depth-fade spike", "photo both passes", 1u, 1u, 500000u, NAV_TOOLS, 0, 0 },
    { "PVRDMA.BIN", "pvr-dma", "PVR-DMA download proof: VRAM to RAM. ~2 s", "photo the page and the graph", 2u, 1u, 400000u, NAV_TOOLS, 0, 0 },
    { "HUD1C2.BIN", "hud1c-2", "TEST, no label: hud1b, no DIAG box in opaque", "photo the bottom two lines", 0u, 0u, 900000u, NAV_TOOLS, 0, 0 },
};
#define N ((unsigned)(sizeof list / sizeof list[0]))

static void unpack(uint16_t v, double *r, double *g, double *b) {
    *r = ((v >> 11) & 31u) / 31.0; *g = ((v >> 5) & 63u) / 63.0; *b = (v & 31u) / 31.0;
}

static double lin(double c) { return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }

/* WCAG's contrast ratio of two RGB565 colours. */
static double contrast(uint16_t a, uint16_t b) {
    double r, g, bl, la, lb;

    unpack(a, &r, &g, &bl); la = 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(bl);
    unpack(b, &r, &g, &bl); lb = 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(bl);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

static int is_text(uint16_t v) {
    unsigned i;

    for (i = 0; i < UI_TXTS; i++) if (v == ui_txt[i]) return 1;
    return 0;
}

static void ppm(const char *name) {
    char path[512];
    FILE *f;
    unsigned i;

    if (!ppm_dir) return;
    snprintf(path, sizeof path, "%s/%s.ppm", ppm_dir, name);
    if (!(f = fopen(path, "wb"))) { printf("FAIL: cannot write %s\n", path); fails++; return; }
    fprintf(f, "P6\n640 480\n255\n");
    for (i = 0; i < 640u * 480u; i++) {
        const unsigned char p[3] = { (unsigned char)(((fb[i] >> 11) & 31u) * 255u / 31u), (unsigned char)(((fb[i] >> 5) & 63u) * 255u / 63u), (unsigned char)((fb[i] & 31u) * 255u / 31u) };

        fwrite(p, 1, 3, f);
    }
    fclose(f);
}

/* One screen: draw it, check it. */
static void screen(const char *name, const nav_t *v, const char *note) {
    static nav_grid_t g;
    nav_span_t span[8];
    unsigned r, i, n, x, y;

    nav_grid(&g, v, list, N, "SH4DER-PVR SUITE s12", note, psm_credits_foot);
    memset(fb, 0, sizeof fb);
    ui_screen(fb, ui_skin, &font, &g, v);
    ppm(name);
#ifndef UI_MINIFONT
    for (y = 0; y < 480u; y++)              /* the text: in the title-safe area */
        for (x = 0; x < 640u; x++)
            if (is_text(fb[y * 640u + x]) && (x < 64u || x > 575u || y < 48u || y > 431u)) { printf("FAIL %s: text at %u, %u\n", name, x, y); fails++; y = 480u; break; }
#endif
    for (r = 0; r < NAV_ROWS; r++) {        /* a panel under every option, background between two */
        const unsigned k = g.kind[r];

        if (k != NAV_K_ITEM && k != NAV_K_PICK) continue;
        CHECK(fb[(UI_ROW_Y(r) + 4) * 640 + UI_PANEL_X1 - 8] == ui_skin[k == NAV_K_PICK ? UI_ROW_PICK : UI_ROW].flat);
        if (r > 0u && (g.kind[r - 1u] == NAV_K_ITEM || g.kind[r - 1u] == NAV_K_PICK || g.kind[r - 1u] == NAV_K_ITEM_MORE || g.kind[r - 1u] == NAV_K_PICK_MORE))
            CHECK(fb[(UI_ROW_Y(r) - 2) * 640 + UI_PANEL_X1 - 2] == ui_skin[UI_BG].flat);
        if (k == NAV_K_PICK) CHECK(fb[(UI_ROW_Y(r) + 4) * 640 + UI_PANEL_X0 - 4] == ui_skin[UI_ROW_MARK].flat);
    }
    n = nav_legend_spans(span, 8u, v->screen, v->has_top, NAV_COLS);
    CHECK(n >= 3u);
    for (i = 0; i < n; i++) {               /* a chip under each key: its colour each side of the key's text */
        const unsigned slot = ui_chip_slot(span[i].buttons);
        const int yy = UI_ROW_Y(NAV_KEYS_ROW) + 6;

        CHECK(fb[yy * 640 + UI_X0 + UI_CW * span[i].col - 2] == ui_skin[slot].flat);
        CHECK(fb[yy * 640 + UI_X0 + UI_CW * (span[i].col + span[i].len) + 1] == ui_skin[slot].flat);
        CHECK(span[i].col + span[i].len <= strlen(g.text[NAV_KEYS_ROW]) && g.text[NAV_KEYS_ROW][span[i].col] != ' ');
    }
    printf("screen %s: drawn\n", name);
}

static void check_contrast(void) {
    static const struct { unsigned txt, slot; } on[] = {
        { UI_TXT_WHITE, UI_BG }, { UI_TXT_GREY, UI_BG }, { UI_TXT_YELLOW, UI_BG }, { UI_TXT_LABEL, UI_BG },
        { UI_TXT_WHITE, UI_BAND }, { UI_TXT_GREY, UI_BAND }, { UI_TXT_YELLOW, UI_BAND },
        { UI_TXT_WHITE, UI_ROW }, { UI_TXT_GREY, UI_ROW },
        { UI_TXT_YELLOW, UI_ROW_PICK }, { UI_TXT_GREY_PICK, UI_ROW_PICK }, { UI_TXT_WHITE, UI_ROW_PICK },
        { UI_TXT_WHITE, UI_FLAG },
        { UI_TXT_WHITE, UI_CHIP_PAD }, { UI_TXT_WHITE, UI_CHIP_A }, { UI_TXT_WHITE, UI_CHIP_B }, { UI_TXT_WHITE, UI_CHIP_Y },
    };
    unsigned i;

    for (i = 0; i < sizeof on / sizeof on[0]; i++) {
        const double c = contrast(ui_txt[on[i].txt], ui_skin[on[i].slot].flat);

        if (c < 4.5) { printf("FAIL contrast: text %u on slot %u is %.2f : 1\n", on[i].txt, on[i].slot, c); fails++; }
        else printf("contrast: text %u on slot %u %.2f : 1\n", on[i].txt, on[i].slot, c);
    }
}

static void check_fill(void) {
    unsigned s, x, y, bytes = 0;

    for (s = 0; s < UI_SLOTS; s++) {
        const ui_slot_t *k = &ui_skin[s];

        if (!k->pix) continue;
        bytes += 2u * k->w * k->h;
        CHECK(k->l + k->r + 1u == k->w && k->t + k->b + 1u == k->h);
        CHECK(k->pix[k->t * k->w + k->l] == k->flat);
        for (y = 0; y < 20u; y++) for (x = 0; x < 20u; x++) fb[y * 640u + x] = 0x1234u;
        ui_fill(fb, k, 0, 0, k->w - 1, k->h - 1);            /* at its own size: the skin, pixel for pixel */
        for (y = 0; y < k->h; y++)
            for (x = 0; x < k->w; x++) {
                const uint16_t want = k->pix[y * k->w + x] == UI_CLEAR ? 0x1234u : k->pix[y * k->w + x];

                if (fb[y * 640u + x] != want) { printf("FAIL slot %u at %u, %u\n", s, x, y); fails++; }
            }
        CHECK(fb[k->h * 640u] == 0x1234u && fb[k->w] == 0x1234u);   /* nothing past it */
        ui_fill(fb, k, 0, 0, k->w - 2, k->h - 1);            /* too small: plain */
        CHECK(fb[0] == k->flat && fb[(k->h - 1u) * 640u] == k->flat);
        ui_fill(fb, k, 0, 0, 99, k->h + 5u);                 /* stretched: the middle is the centre colour */
        CHECK(fb[((k->h + 5u) / 2u) * 640u + 50u] == k->flat);
    }
    CHECK(bytes < 2048u);                                    /* the cost line: under 2 KB of RAM */
    printf("skin: %u bytes of 9-slices\n", bytes);
    ui_fill(fb, &ui_skin[UI_ROW], 630, 470, 700, 500);       /* clipped to the screen, no write past it */
    CHECK(fb[479u * 640u + 639u] != 0u);
}

static void check_text(void) {
    static uint8_t one[96][16][8];
    const ui_font_t f = { &one[0][0][0], 8u, 16u };

    one['A' - 32][0][0] = 255u; one['A' - 32][0][1] = 128u;
    fb[0] = UI_RGB(0, 0, 0); fb[1] = UI_RGB(0, 0, 0); fb[2] = 0x5555u;
    ui_text(fb, &f, 1u, 0, 0, UI_RGB(248, 252, 248), "A", 1u);
    CHECK(fb[0] == UI_RGB(248, 252, 248));                    /* full cover: the colour */
    CHECK(((fb[1] >> 11) & 31u) - 15u <= 1u && ((fb[1] >> 5) & 63u) - 31u <= 2u);   /* half: half way from what was under (shifts: within a step) */
    CHECK(fb[2] == 0x5555u);                                  /* none: left */
    fb[0] = 0x0000u;
    ui_text(fb, &f, 2u, 0, 0, 0xffffu, "A", 1u);
    CHECK(fb[0] == 0xffffu && fb[1] == 0xffffu && fb[640] == 0xffffu && fb[641] == 0xffffu);   /* twice the size */
    fb[636] = 0x4321u;
    ui_text(fb, &f, 1u, 636, 0, 0xffffu, "AA", 2u);           /* a character past the edge is not drawn */
    CHECK(fb[636] == 0x4321u);
}

int main(int argc, char **argv) {
    nav_t v;
    int index;
    unsigned c, x, y;

    if (argc > 2 && !strcmp(argv[1], "-ppm")) ppm_dir = argv[2];
    for (c = 33u; c < 127u; c++)
        for (y = 0; y < 16u; y++)
            for (x = 0; x < 8u; x++) {
#ifdef UI_MINIFONT
                glyph[c - 32u][y][x] = (minifont_data[(c - 33u) * 16u + y] >> (7u - x)) & 1u ? 255u : 0u;
#else
                glyph[c - 32u][y][x] = x < 7u && y < 15u ? 255u : 0u;
#endif
            }

    check_contrast();
    check_fill();
    check_text();
    CHECK(UI_PANEL_X0 - 6 >= 32 && UI_PANEL_X1 <= 607);      /* panels and marks in the action-safe area */
    CHECK(UI_ROW_Y(NAV_KEYS_ROW) + UI_CH <= 455);

    nav_init(&v, list, N, 0xffffffffu);
    screen("top", &v, "");
    nav_key(&v, list, N, NAV_DOWN, 7u, &index);
    screen("top_tools", &v, "");
    nav_key(&v, list, N, NAV_UP, 7u, &index);
    nav_key(&v, list, N, NAV_A, 7u, &index);
    screen("list_runall", &v, "");
    nav_key(&v, list, N, NAV_DOWN, 7u, &index);
    screen("list_titled", &v, "last: ao4 ended");
    nav_key(&v, list, N, NAV_DOWN, 7u, &index);
    nav_key(&v, list, N, NAV_DOWN, 7u, &index);
    screen("list_untitled", &v, "");
    nav_key(&v, list, N, NAV_B, 7u, &index);
    nav_key(&v, list, N, NAV_DOWN, 7u, &index);
    nav_key(&v, list, N, NAV_A, 7u, &index);
    for (c = 0; c < 3u; c++) nav_key(&v, list, N, NAV_DOWN, 7u, &index);
    screen("list_noreturn", &v, "RUN ALL stopped after pvr-dma");
    {   /* a redraw of the changed lines only gives the same picture as a redraw of all of it, every key on every screen */
        static uint16_t part[640 * 480];
        static nav_grid_t ga, gb;
        static const unsigned keys[] = { NAV_DOWN, NAV_DOWN, NAV_A, NAV_DOWN, NAV_DOWN, NAV_DOWN, NAV_UP, NAV_B, NAV_DOWN, NAV_A,
                                         NAV_DOWN, NAV_DOWN, NAV_DOWN, NAV_DOWN, NAV_UP, NAV_START, NAV_UP, NAV_A, NAV_UP };
        unsigned lines = 0, most = 0;
        int y0, y1;

        nav_init(&v, list, N, 0xffffffffu);
        nav_grid(&ga, &v, list, N, "SH4DER-PVR SUITE s12", "", psm_credits_foot);
        ui_clip(0, 479); ui_screen(part, ui_skin, &font, &ga, &v);
        for (c = 0; c < sizeof keys / sizeof keys[0]; c++) {
            nav_key(&v, list, N, keys[c], 7u, &index);
            nav_grid(&gb, &v, list, N, "SH4DER-PVR SUITE s12", c == 9u ? "last: d1 ended" : "", psm_credits_foot);
            if (ui_dirty(&ga, &gb, &y0, &y1)) {
                ui_clip(y0, y1); ui_screen(part, ui_skin, &font, &gb, &v);
                lines += (unsigned)(y1 - y0 + 1);
                if ((unsigned)(y1 - y0 + 1) > most) most = (unsigned)(y1 - y0 + 1);
            }
            ui_clip(0, 479); memset(fb, 0, sizeof fb); ui_screen(fb, ui_skin, &font, &gb, &v);
            if (memcmp(fb, part, sizeof fb)) { printf("FAIL: a part redraw after key %u differs from a full one\n", c); fails++; }
            ga = gb;
        }
        CHECK(!ui_dirty(&ga, &gb, &y0, &y1));
        printf("part redraws: %u keys, %u lines in all, %u at most, against %u for whole screens\n", c, lines, most, c * 480u);
    }
    ui_clip(0, 479);
    memset(fb, 0, sizeof fb);
    ui_frame(fb, ui_skin);
    CHECK(fb[240 * 640 + 320] == ui_skin[UI_BG].flat && fb[20 * 640 + 320] == ui_skin[UI_BAND].flat && fb[470 * 640 + 320] == ui_skin[UI_BAND].flat);
    ppm("frame");

    if (fails) printf("suite_ui_test: %d FAILED\n", fails); else printf("suite_ui_test: ok\n");
    return fails ? 1 : 0;
}
