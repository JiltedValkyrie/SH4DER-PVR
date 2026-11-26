/* SH4DER-PVR: host test of every program's effect label table (demo/psm and demo/pl4nar), through the label kit
   include/sh4der_fxlabel.h

   demo/psm/test/fx_tables_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The tables are the demos' own: demo/psm/ao/ao_fx.h, demo/psm/hero/hero_fx.h, demo/pl4nar/src/pl4nar_fx.h. Each is run
   for every mode it has, and a table that does not fit two lines of 64 characters FAILS here, at build time. A program that
   adds a table adds its loop to tables() below. The rules and the formatter are test/fxlabel_test.c's.

   Compile: cc -std=c11 -Wall -Wextra -Werror -Iinclude demo/psm/test/fx_tables_test.c -o build/fx_tables_test
   -v prints every text. */

#include <stdio.h>
#include <string.h>

#include "../../../include/sh4der_fxlabel.h"
#include "../ao/ao_fx.h"
#include "../hero/hero_fx.h"
#include "../../pl4nar/src/pl4nar_fx.h"

static int fails, verbose;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void show(const char *what, const fx_text_t *t) {
    unsigned l;

    if (!verbose) return;
    printf("  %s\n", what);
    for (l = 0; l < t->lines; l++) printf("    |%-64s|\n", t->line[l]);
}

/* What must hold for any text: the widths, the spans on the word, every entry placed when it fits. */
static void sane(const char *what, const fx_text_t *t) {
    unsigned l, i;

    show(what, t);
    CHECK(t->fits, "%s: does not fit (%u of %u entries)", what, t->placed, t->total);
    CHECK(t->lines <= FX_LINES, "%s: %u lines", what, t->lines);
    for (l = 0; l < FX_LINES_MAX; l++) CHECK(strlen(t->line[l]) <= FX_COLS, "%s: line %u is %u long", what, l, (unsigned)strlen(t->line[l]));
    if (t->fits) CHECK(t->placed == t->total, "%s: placed %u of %u", what, t->placed, t->total);
    for (i = 0; i < t->spans; i++) {
        const fx_span_t *s = &t->span[i];

        CHECK(s->c1 - s->c0 == 10 && !strncmp(t->line[s->line] + s->c0, "Realtime<<", 10u), "%s: span %u is not on Realtime<<", what, i);
    }
    for (l = 0; l < t->lines; l++) {        /* every Realtime<< has a span */
        const char *p = t->line[l];

        while ((p = strstr(p, "Realtime<<")) != NULL) { CHECK(fx_marked(t, l, (unsigned)(p - t->line[l])), "%s: a Realtime with no span", what); p++; }
    }
}

/* Every table a program has, every mode. */
static void tables(void) {
    static const char *const mname[5] = { "OFF", "1", "2", "3", "3+M" };
    fx_entry_t l[3], r[3];
    fx_text_t t;
    unsigned ml, mr, sun, mov, nl, nr;
    char what[96];

    /* ao4: scenes A, B have methods 0 .. 2 with and without the sun's shadow; the room 0 .. 4, never a sun shadow */
    for (sun = 0; sun < 2u; sun++)
        for (ml = 0; ml < 5u; ml++)
            for (mr = 0; mr < 5u; mr++)
                for (mov = 0; mov < 2u; mov++) {
                    if (sun && (ml > 2u || mr > 2u)) continue;
                    nl = ao4_fx_half(ml, (int)sun, (int)mov, l); nr = ao4_fx_half(mr, (int)sun, (int)mov, r);
                    snprintf(what, sizeof what, "ao4 %s | %s%s%s", mname[ml], mname[mr], sun ? ", sun shadow" : "", mov ? ", hens" : "");
                    fx_format_split(&t, l, nl, r, nr); sane(what, &t);
                    CHECK(nl <= 3u && nr <= 3u, "%s: a half has more than 3 entries", what);
                }
    nl = ao4_fx_half(4u, 0, 1, l); nr = ao4_fx_half(2u, 0, 1, r);
    fx_format_split(&t, l, nl, r, nr);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion: L Lightmap PVR Texture,") && !strcmp(t.line[1], "L movers SH-4/PVR Realtime<<, R SH-4/PVR Realtime<<"), "ao4 3+M | 2 reads '%s' / '%s'", t.line[0], t.line[1]);
    nl = ao4_fx_half(0u, 1, 0, l); nr = ao4_fx_half(1u, 1, 0, r);
    fx_format_split(&t, l, nl, r, nr);
    CHECK(!strcmp(t.line[0], "Projected Shadow: L+R SH-4/PVR Realtime<<") && !strcmp(t.line[1], "Ambient Occlusion: R SH-4 Baked"), "ao4 OFF | 1 reads '%s' / '%s'", t.line[0], t.line[1]);
    fx_format_page(&t, ao4_fx_page, AO4_FX_PAGE_N); sane("ao4 page", &t);
    CHECK(t.lines == 2u, "ao4's page is two lines (PG_TOP 1): %u", t.lines);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion: SH-4 Baked, PVR Texture, SH-4/PVR Realtime<<"), "ao4's page reads '%s'", t.line[0]);

    fx_format(&t, hen_fx_eyes, HEN_FX_N); sane("hens-eyes EYES", &t);
    fx_format(&t, hen_fx_sweep, HEN_FX_N); sane("hens-eyes SWEEP", &t);
    fx_format_page(&t, hen_fx_page, HEN_FX_PAGE_N); sane("hens-eyes page", &t);
    CHECK(t.lines == 2u, "hens-eyes' page is two lines (PG_TOP 1): %u", t.lines);
    {   /* ao-lamp: every state of the live view in two lines, the page in three */
        fx_entry_t e[6];
        unsigned k;

        for (k = 0; k < 8u; k++) {
            const unsigned n = lamp_fx_live((int)(k & 1u), (int)(k >> 1 & 1u), (int)(k >> 2 & 1u), e);

            snprintf(what, sizeof what, "ao-lamp lamp %u slots %u light %u", k & 1u, k >> 1 & 1u, k >> 2 & 1u);
            if ((k & 7u) == 7u) {           /* all six: more than two lines hold; the program never shows them together */
                CHECK(!fx_format_merged(&t, e, n), "six entries were not expected to fit two lines");
                continue;
            }
            fx_format_merged(&t, e, n); sane(what, &t);
            CHECK(t.lines <= 2u, "%s: %u lines", what, t.lines);
        }
        fx_format_merged(&t, e, lamp_fx_live(1, 1, 0, e));
        CHECK(!strcmp(t.line[0], "Emission, Subsurface Scattering, Glow: SH-4 Realtime<<") && !strcmp(t.line[1], "Ambient Occlusion SH-4 Baked | Punch-Through Cutout PVR Texture"),
              "ao-lamp with slots reads '%s' / '%s'", t.line[0], t.line[1]);
        fx_format_merged(&t, e, lamp_fx_live(1, 0, 1, e));
        CHECK(!strcmp(t.line[0], "Emission, Subsurface Scattering, Glow,") && !strcmp(t.line[1], "Point Light: SH-4 Realtime<< | Ambient Occlusion SH-4 Baked"),
              "ao-lamp with the room's light reads '%s' / '%s'", t.line[0], t.line[1]);
        CHECK(fx_format_page_n(&t, lamp_fx_page, LAMP_FX_PAGE_N, 3u) && t.lines == 3u, "ao-lamp's page is three lines (PG_TOP 2): %u", t.lines);
        show("ao-lamp page", &t);
        CHECK(!fx_format_page_n(&t, lamp_fx_page, LAMP_FX_PAGE_N, 2u), "ao-lamp's page does not fit two lines");
    }
    fx_format_page(&t, pd_fx_page, PD_FX_PAGE_N); sane("pvr-dma page", &t);
    CHECK(t.lines == 1u, "pvr-dma's page is one line: %u", t.lines);

    {   /* hero1: every switch of the live view (hero, detail, visor, shadow, room), and the page */
        fx_entry_t h[HERO_FX_MAX];
        unsigned k;
        for (k = 0; k < 32u; k++) {
            const unsigned n = hero_fx_live((int)(k & 1u), (int)(k >> 1 & 1u), (int)(k >> 2 & 1u), (int)(k >> 3 & 1u), (int)(k >> 4 & 1u), h);
            snprintf(what, sizeof what, "hero1 hero %u det %u vis %u shd %u room %u", k & 1u, k >> 1 & 1u, k >> 2 & 1u, k >> 3 & 1u, k >> 4 & 1u);
            if (!n) continue;
            fx_format(&t, h, n); sane(what, &t);
        }
        fx_format_page(&t, hero_fx_page, HERO_FX_PAGE_N); sane("hero1 page", &t);
        CHECK(t.lines == 2u, "hero1's page is two lines (PG_TOP 1): %u", t.lines);
    }
    {   /* pl4nar: every switch of the live view (reflect, tiles, blobs, ao, night; tiles by day only), merged as the
           demo has it, the split (the same text: the side labels carry the difference), and the page (PG_TOP 2) */
        fx_entry_t l[PL4NAR_FX_MAX];
        unsigned k;
        for (k = 0; k < 32u; k++) {
            const int tl = (int)(k >> 1 & 1u), bl = (int)(k >> 2 & 1u), ao = (int)(k >> 3 & 1u), nt = (int)(k >> 4 & 1u);
            const unsigned n = pl4nar_fx_live((int)(k & 1u), tl, bl, ao, nt, l);
            if (tl && nt) continue;
            snprintf(what, sizeof what, "pl4nar refl %u tiles %d blobs %d ao %d night %d", k & 1u, tl, bl, ao, nt);
            if (n) { fx_format_merged(&t, l, n); sane(what, &t); }
        }
        fx_format_page_n(&t, pl4nar_fx_page, PL4NAR_FX_PAGE_N, 3u); show("pl4nar page", &t);
        CHECK(t.fits && t.lines <= 3u, "pl4nar's page fits three lines (PG_TOP 2): fits %d, %u lines", t.fits, t.lines);
    }
}

int main(int argc, char **argv) {
    verbose = argc > 1 && !strcmp(argv[1], "-v");
    tables();
    if (fails) printf("fx_tables_test: %d FAILED\n", fails); else printf("fx_tables_test: ok\n");
    return fails ? 1 : 0;
}
