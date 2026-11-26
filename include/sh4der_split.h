/* SH4DER-PVR: a screen cut in a left and a right half by a divider the triggers move: the column maths

   sh4der_split.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Plain C: no maths library, so no SH4ZAM call (the only float step is an integer to float convert).

   RECOMMENDED (from the wiki ledger; wiki/limits/header-recommended.md)
   Start with: SH4DER_SPLIT_TILE 32, SH4DER_SPLIT_COLS 20, SH4DER_SPLIT_ROWS 15 (a 640 x 480 screen),
               SH4DER_SPLIT_REPEAT 6 (the whole way in 2 s); the divider at sh4der_split_middle(SH4DER_SPLIT_COLS)
   Cost:       not measured. The maths is integer compares a frame; the cost is the two halves' own sends
   Status:     ADVISED. The middle position works on a console (PVR-144, PVR-161: the split's user clip); a
               divider that moves has no console run (wiki/status/maturity.md "split divider": OPEN)
   Do:         set each half's clip every frame, with the dummy polygon before each change of clip
   Do not:     send a hidden half at all, or give it a clip rectangle (a rectangle cannot be empty)
   Maturity:   ALPHA v0.2

   For a demo that compares two looks side by side: the same scene sent twice, each half inside its own user tile
   clip. The PVR's user clip is in tiles of 32 pixels, so the divider has cols + 1 places (21 on a 640 pixel screen):

     d = 0 .. cols      the tile column where the right half starts; the edge is at pixel 32 x d
     left half          tile columns 0 .. d - 1          right half        tile columns d .. cols - 1
     d = 0              the left half is hidden          d = cols          the right half is hidden

   A hidden half has no clip rectangle (a rectangle cannot be empty): do not send that half at all. That also saves
   its send, and any pass that only it used.

   This header is the maths and the held-trigger stepping. It sends nothing: the clip itself is the program's call
   (ao4_spike.c's user_clip, or sh4der_pvrx_clip_set once that layer has its console row), with the dummy polygon
   before each change of clip (the core takes a new clip one polygon early). A divider that moves needs nothing more:
   the clip is a parameter sent into the list, and a program sets it for each half every frame already.
   Status: host tested (test/split_test.c). A divider that moves has not run on a console.

   USE, a frame:
       d = sh4der_split_step(&step, d, SH4DER_SPLIT_COLS, lt_down, rt_down);       both triggers down: no move (a chord's)
       sh4der_split_halves(d, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &left, &right);
       if (!left.hidden)  { clip(left.tx0, left.ty0, left.tx1, left.ty1);     ... send the left look ... }
       if (!right.hidden) { clip(right.tx0, right.ty0, right.tx1, right.ty1); ... send the right look ... }
       the dividing line at sh4der_split_x(d), unless sh4der_split_at_end(d, SH4DER_SPLIT_COLS)
       a side's label at sh4der_split_label_x(&left, width): centred on its half, kept inside the title-safe area

   Plain C, no KallistiOS, no maths library. */

#ifndef SH4DER_SPLIT_H
#define SH4DER_SPLIT_H

#define SH4DER_SPLIT_TILE 32u
#define SH4DER_SPLIT_COLS 20u                      /* 640 / 32 */
#define SH4DER_SPLIT_ROWS 15u                      /* 480 / 32 */
#define SH4DER_SPLIT_REPEAT 6u                     /* frames between two steps while a trigger is held: the whole way in 2 s */
#define SH4DER_SPLIT_SAFE_X0 64u                   /* the title-safe area's sides, as demo/psm/psm_page.h has them */
#define SH4DER_SPLIT_SAFE_X1 576u

typedef struct sh4der_split_half {
    unsigned tx0, ty0, tx1, ty1;            /* the clip's tiles, both ends counted */
    int hidden;                             /* 1: no tile; the rectangle is not valid, send nothing */
} sh4der_split_half_t;

typedef struct sh4der_split_step {
    int dir;                                /* the trigger held: -1 left, 1 right, 0 none or both */
    unsigned frames;                        /* since the last step */
} sh4der_split_step_t;

static inline unsigned sh4der_split_middle(unsigned cols) { return cols / 2u; }
static inline int sh4der_split_at_end(unsigned d, unsigned cols) { return d == 0u || d >= cols; }
static inline float sh4der_split_x(unsigned d) { return (float)(d * SH4DER_SPLIT_TILE); }

/* The two halves' clip rectangles for divider d (held to 0 .. cols). */
static inline void sh4der_split_halves(unsigned d, unsigned cols, unsigned rows, sh4der_split_half_t *left, sh4der_split_half_t *right) {
    if (d > cols) d = cols;
    left->hidden = d == 0u; right->hidden = d == cols;
    left->tx0 = 0u; left->ty0 = 0u; left->tx1 = d ? d - 1u : 0u; left->ty1 = rows ? rows - 1u : 0u;
    right->tx0 = d < cols ? d : cols ? cols - 1u : 0u; right->ty0 = 0u; right->tx1 = cols ? cols - 1u : 0u; right->ty1 = rows ? rows - 1u : 0u;
}

/* The divider after this frame. lt, rt: the triggers as buttons (down or not). One step when a trigger goes down,
   then one every SH4DER_SPLIT_REPEAT frames while it stays down; both down, or none: no move. Stops at 0 and at cols. */
static inline unsigned sh4der_split_step(sh4der_split_step_t *s, unsigned d, unsigned cols, int lt, int rt) {
    const int dir = lt && !rt ? -1 : rt && !lt ? 1 : 0;
    int move = 0;

    if (d > cols) d = cols;
    if (dir != s->dir) { s->dir = dir; s->frames = 0u; move = dir != 0; }
    else if (dir && ++s->frames >= SH4DER_SPLIT_REPEAT) { s->frames = 0u; move = 1; }
    if (!move) return d;
    if (dir < 0) return d ? d - 1u : 0u;
    return d < cols ? d + 1u : cols;
}

/* Where a label `width` pixels wide starts so that it is centred on a half and stays inside the title-safe area.
   -1 when the half is hidden or narrower than the label's room there. */
static inline int sh4der_split_label_x(const sh4der_split_half_t *h, unsigned width) {
    unsigned x0, x1;

    if (h->hidden) return -1;
    x0 = h->tx0 * SH4DER_SPLIT_TILE; x1 = (h->tx1 + 1u) * SH4DER_SPLIT_TILE;
    if (x0 < SH4DER_SPLIT_SAFE_X0) x0 = SH4DER_SPLIT_SAFE_X0;
    if (x1 > SH4DER_SPLIT_SAFE_X1) x1 = SH4DER_SPLIT_SAFE_X1;
    if (x1 <= x0 || x1 - x0 < width) return -1;
    return (int)(x0 + (x1 - x0 - width) / 2u);
}

#endif /* SH4DER_SPLIT_H */
