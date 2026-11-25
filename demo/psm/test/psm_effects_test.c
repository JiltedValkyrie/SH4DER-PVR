/* SH4DER-PVR: host test of the effect label line (demo/psm/psm_effects.h) and of every program's table that is in

   demo/psm/test/psm_effects_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The text's form, the merge by name, the break between lines, the Realtime spans, the chord; then every table of
   ao/ao_fx.h for every mode it has: a table that does not fit two lines of 64 characters FAILS here, at build time.
   A program that adds a table adds its loop to tables() below.

   Compile: cc -std=c11 -Wall -Wextra -Werror -Idemo/psm demo/psm/test/psm_effects_test.c -o psm_effects_test
   -v prints every text. */

#include <stdio.h>
#include <string.h>

#include "../psm_effects.h"
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

static void form(void) {
    static const fx_entry_t lead[] = { { "Ambient Occlusion", FX_SH4, FX_BAKED, NULL }, { "Shadow Mapping", FX_PVR, FX_REALTIME, NULL },
                                       { "Subsurface Scattering", FX_SH4, FX_REALTIME, NULL } };
    static const fx_entry_t two[] = { { "Ambient Occlusion", FX_SH4, FX_BAKED, NULL }, { "Fog", FX_PVR, FX_REALTIME, NULL } };
    static const fx_entry_t same[] = { { "Ambient Occlusion", FX_SH4, FX_BAKED, NULL }, { "Ambient Occlusion", FX_PVR, FX_TEXTURE, "Lightmap" },
                                       { "Fog", FX_PVR, FX_REALTIME, NULL }, { "Ambient Occlusion", FX_SH4, FX_BAKED, NULL } };
    static const fx_entry_t one[] = { { "Ambient Occlusion", FX_PVR, FX_TEXTURE, "Lightmap" } };
    static const fx_entry_t none[] = { { "No effect: a test", FX_NONE, 0, NULL } };
    fx_entry_t many[FX_MAX];
    fx_text_t t;
    unsigned i;
    int hidden = 0;

    fx_format(&t, lead, 3u); sane("the example line", &t);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion  SH-4  Baked | Shadow Mapping  PVR  Realtime<<"), "line 0 reads '%s'", t.line[0]);
    CHECK(!strcmp(t.line[1], "Subsurface Scattering  SH-4  Realtime<<"), "line 1 reads '%s'", t.line[1]);
    CHECK(t.lines == 2u && t.spans == 2u, "lines %u spans %u", t.lines, t.spans);
    CHECK(t.span[0].line == 0u && t.span[0].c1 == 64u && t.span[1].line == 1u, "the spans' places");

    fx_format(&t, two, 2u); sane("two on a line", &t);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion  SH-4  Baked | Fog  PVR  Realtime<<") && t.lines == 1u, "reads '%s'", t.line[0]);

    fx_format(&t, same, 4u); sane("a name three times, one of them twice the same", &t);
    CHECK(t.total == 3u, "the repeated entry counted: %u", t.total);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion: SH-4 Baked, Lightmap PVR Texture") && !strcmp(t.line[1], "Fog  PVR  Realtime<<"), "reads '%s' / '%s'", t.line[0], t.line[1]);

    fx_format(&t, one, 1u); sane("a lone entry with a detail", &t);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion (Lightmap)  PVR  Texture"), "reads '%s'", t.line[0]);
    fx_format_page(&t, one, 1u);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion  PVR  Texture"), "a page drops the detail: '%s'", t.line[0]);

    fx_format(&t, none, 1u); sane("no effect", &t);
    CHECK(!strcmp(t.line[0], "No effect: a test") && t.spans == 0u, "reads '%s'", t.line[0]);

    fx_format(&t, NULL, 0u);
    CHECK(t.fits && t.lines == 0u && !t.line[0][0], "an empty table is no line");

    fx_format_split(&t, two, 1u, two, 2u); sane("a split with a shared entry", &t);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion: L+R SH-4 Baked | Fog: R PVR Realtime<<"), "reads '%s'", t.line[0]);
    fx_format_split(&t, NULL, 0u, two, 1u); sane("a split with an empty half", &t);
    CHECK(!strcmp(t.line[0], "Ambient Occlusion: R SH-4 Baked | No effect: L"), "reads '%s'", t.line[0]);

    /* the gap narrows before a table is called too long */
    {
        static const fx_entry_t tight[] = { { "Subsurface Scattering", FX_BOTH, FX_REALTIME, NULL }, { "Fog", FX_PVR, FX_BAKED, NULL },
                                            { "Planar Reflection", FX_BOTH, FX_REALTIME, NULL }, { "Emission", FX_SH4, FX_TEXTURE, NULL } };

        fx_format(&t, tight, 4u); sane("narrow gaps", &t);
        CHECK(!strcmp(t.line[1], "Planar Reflection SH-4/PVR Realtime<< | Emission SH-4 Texture"), "the narrow form: '%s'", t.line[1]);
    }

    /* too long: says so, shortens nothing, keeps what fitted whole */
    for (i = 0; i < FX_MAX; i++) { many[i].name = i & 1u ? "Subsurface Scattering" : "Planar Reflection"; many[i].who = (uint8_t)(i % 3u); many[i].when = (uint8_t)(i / 3u % 3u); many[i].detail = NULL; }
    CHECK(!fx_format(&t, many, FX_MAX), "sixteen entries must not fit");
    CHECK(!t.fits && t.placed < t.total && t.placed > 0u, "placed %u of %u", t.placed, t.total);
    CHECK(strlen(t.line[0]) <= FX_COLS && strlen(t.line[1]) <= FX_COLS, "an over-long table keeps its lines' width");
    {
        static const fx_entry_t wide[] = { { "A name that is far too long to go on a line of sixty-four characters at all", FX_SH4, FX_BAKED, NULL } };

        CHECK(!fx_format(&t, wide, 1u) && !t.line[0][0], "a name over a line is refused whole");
    }

    CHECK(!fx_hide_chord(0, 1, &hidden) && !hidden, "the button alone does not hide");
    CHECK(!fx_hide_chord(1, 0, &hidden) && !hidden, "the modifier alone does not hide");
    CHECK(fx_hide_chord(1, 1, &hidden) && hidden, "the chord hides");
    CHECK(fx_hide_chord(1, 1, &hidden) && !hidden, "and shows again");
    CHECK(!strcmp(fx_who_text(FX_BOTH), "SH-4/PVR") && !strcmp(fx_when_text(FX_REALTIME), "Realtime<<") && !strcmp(fx_when_text(FX_TEXTURE), "Texture"), "the words");
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
    form();
    tables();
    if (fails) printf("psm_effects_test: %d FAILED\n", fails); else printf("psm_effects_test: ok\n");
    return fails ? 1 : 0;
}
