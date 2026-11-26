/* SH4DER-PVR: host test of the effect label rules and formatter (include/sh4der_fxlabel.h)

   test/fxlabel_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   The text's form, the merge by name, the break between lines, the Realtime spans, the chord. Every program's own table
   is in demo/psm/test/fx_tables_test.c (it needs the demo's table headers, so it runs in make test-demo).

   Compile: cc -std=c11 -Wall -Wextra -Werror -Iinclude test/fxlabel_test.c -o build/fxlabel_test
   -v prints every text. */

#include <stdio.h>
#include <string.h>

#include "../include/sh4der_fxlabel.h"

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

int main(int argc, char **argv) {
    verbose = argc > 1 && !strcmp(argv[1], "-v");
    form();
    if (fails) printf("fxlabel_test: %d FAILED\n", fails); else printf("fxlabel_test: ok\n");
    return fails ? 1 : 0;
}
