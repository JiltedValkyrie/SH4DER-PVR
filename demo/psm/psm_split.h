/* SH4DER-PVR: a screen cut in a left and a right half by a divider the triggers move (demo/psm): the column maths

   demo/psm/psm_split.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

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
   Status: host tested (test/psm_split_test.c). A divider that moves has not run on a console.

   USE, a frame:
       d = split_step(&step, d, SPLIT_COLS, lt_down, rt_down);       both triggers down: no move (a chord's)
       split_halves(d, SPLIT_COLS, SPLIT_ROWS, &left, &right);
       if (!left.hidden)  { clip(left.tx0, left.ty0, left.tx1, left.ty1);     ... send the left look ... }
       if (!right.hidden) { clip(right.tx0, right.ty0, right.tx1, right.ty1); ... send the right look ... }
       the dividing line at split_x(d), unless split_at_end(d, SPLIT_COLS)
       a side's label at split_label_x(&left, width): centred on its half, kept inside the title-safe area

   Plain C, no KallistiOS, no maths library. */

#ifndef PSM_SPLIT_H
#define PSM_SPLIT_H

#define SPLIT_TILE 32u
#define SPLIT_COLS 20u                      /* 640 / 32 */
#define SPLIT_ROWS 15u                      /* 480 / 32 */
#define SPLIT_REPEAT 6u                     /* frames between two steps while a trigger is held: the whole way in 2 s */
#define SPLIT_SAFE_X0 64u                   /* the title-safe area's sides, as ../psm_page.h has them */
#define SPLIT_SAFE_X1 576u

typedef struct split_half {
    unsigned tx0, ty0, tx1, ty1;            /* the clip's tiles, both ends counted */
    int hidden;                             /* 1: no tile; the rectangle is not valid, send nothing */
} split_half_t;

typedef struct split_step {
    int dir;                                /* the trigger held: -1 left, 1 right, 0 none or both */
    unsigned frames;                        /* since the last step */
} split_step_t;

static inline unsigned split_middle(unsigned cols) { return cols / 2u; }
static inline int split_at_end(unsigned d, unsigned cols) { return d == 0u || d >= cols; }
static inline float split_x(unsigned d) { return (float)(d * SPLIT_TILE); }

/* The two halves' clip rectangles for divider d (held to 0 .. cols). */
static inline void split_halves(unsigned d, unsigned cols, unsigned rows, split_half_t *left, split_half_t *right) {
    if (d > cols) d = cols;
    left->hidden = d == 0u; right->hidden = d == cols;
    left->tx0 = 0u; left->ty0 = 0u; left->tx1 = d ? d - 1u : 0u; left->ty1 = rows ? rows - 1u : 0u;
    right->tx0 = d < cols ? d : cols ? cols - 1u : 0u; right->ty0 = 0u; right->tx1 = cols ? cols - 1u : 0u; right->ty1 = rows ? rows - 1u : 0u;
}

/* The divider after this frame. lt, rt: the triggers as buttons (down or not). One step when a trigger goes down,
   then one every SPLIT_REPEAT frames while it stays down; both down, or none: no move. Stops at 0 and at cols. */
static inline unsigned split_step(split_step_t *s, unsigned d, unsigned cols, int lt, int rt) {
    const int dir = lt && !rt ? -1 : rt && !lt ? 1 : 0;
    int move = 0;

    if (d > cols) d = cols;
    if (dir != s->dir) { s->dir = dir; s->frames = 0u; move = dir != 0; }
    else if (dir && ++s->frames >= SPLIT_REPEAT) { s->frames = 0u; move = 1; }
    if (!move) return d;
    if (dir < 0) return d ? d - 1u : 0u;
    return d < cols ? d + 1u : cols;
}

/* Where a label `width` pixels wide starts so that it is centred on a half and stays inside the title-safe area.
   -1 when the half is hidden or narrower than the label's room there. */
static inline int split_label_x(const split_half_t *h, unsigned width) {
    unsigned x0, x1;

    if (h->hidden) return -1;
    x0 = h->tx0 * SPLIT_TILE; x1 = (h->tx1 + 1u) * SPLIT_TILE;
    if (x0 < SPLIT_SAFE_X0) x0 = SPLIT_SAFE_X0;
    if (x1 > SPLIT_SAFE_X1) x1 = SPLIT_SAFE_X1;
    if (x1 <= x0 || x1 - x0 < width) return -1;
    return (int)(x0 + (x1 - x0 - width) / 2u);
}

#endif /* PSM_SPLIT_H */
