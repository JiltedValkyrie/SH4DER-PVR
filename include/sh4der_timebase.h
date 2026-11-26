/* SH4DER-PVR: time base

   sh4der_timebase.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   The time-base model in this file is a port to C and single precision of parts of
   NTSCRT (https://github.com/finnmckenty/NTSCRT), port/engine/src/glitch/receiver.rs:
   the hash and noise functions (mix, uniform, gauss, value), the deck's time-base
   error (head switching, drum flutter, tape tension skew) and the television's
   horizontal lock loop. NTSCRT is:

   MIT License

   Copyright (c) 2026 Finn McKenty

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/* RECOMMENDED (no console run on record: nothing below is measured; wiki/limits/header-recommended.md)
   Start with: the short film's deck (640 pixels wide, 360 rows): tension = 0.375 (a hook of about 24 pixels at the
               top row), flutter = 0.135, head_switch_us = 0.6 shown 1.5 s in every 20 s, afc_speed = 0, amount = 1;
               a mesh row every 6 picture rows; an overscan of 8 pixels; SH4DER_CA_ROWS_MESH_X = 2
   Cost:       counts, not measured: two sines a line and, with line_noise on, a logarithm and six hashes a line;
               about 4.2 KB of stack in sh4der_timebase_field; RAM 40 B and 4 B of state; VRAM 0 of its own (the
               frame texture is sh4der_ca.h's: 640 KB at 640 x 480). The screen pass through
               sh4der_ca_channel_rows is 4 vertices a mesh row a channel
   Status:     ADVISED (NOT RUN in the ledger); host tested (test/timebase_test.c). To prove: the same shot with
               the bend on and off, frame, SH-4 and PVR time
   Do:         call sh4der_timebase_field once a displayed frame, fields in order, from the main thread; give
               sh4der_ca_channel_rows an overscan as large as the largest shift; keep the notice above and credit
               NTSCRT and Finn McKenty in the program that uses it
   Do not:     call it from a small worker stack or an interrupt; draw a HUD or text through the bent pass
   Maturity:   PROTOTYPE v0.1 */

/** \file    sh4der_timebase.h
    \brief   A video tape deck's line timing errors as a television shows them:
             how far each row of the picture sits sideways, field by field.

    A tape deck never delivers the lines of a picture exactly on time. This
    module works out, for one field (one displayed frame), how late or early
    each line arrives and where the television, which follows the lines'
    timing with a slow loop of its own, therefore draws each row:

    - Head switching: a deck's two video heads take turns, a field each, and
      sit a hair apart on the drum. Every line of one head arrives a fixed
      time later than the other's, and the change-over, a few lines above
      the bottom of the picture, shows as a step: the bottom rows jump
      sideways.
    - Drum flutter: the drum's speed wavers (a wobble once a turn, slow
      hunting of its servo, faster roughness), and the rows weave with it.
    - Tension skew: a tape pulled tighter or slacker than when it was
      recorded delivers its lines later and later (or earlier) down the
      field, then snaps back at the head switch. The television's loop has
      to catch up at the top of each picture: the top rows hook sideways.
    - The television's horizontal lock: a loop that steers each scanned
      line toward the timing of the lines before it. It follows slow errors
      and cannot follow sudden ones; what it does not follow is what shows.

    The result is one sideways shift in pixels for every row. A client draws
    its finished frame (a frame texture) as a mesh of horizontal bands and
    moves each band's vertices by its row's shift.

    Not ported from the model this follows: the capstan servo and the tape's
    stick-slip, creases, dubbing generations, picture search, signal noise
    and hum in the lock loop, the horizontal hold control (centred here), and
    the vertical oscillator (no roll: row y always shows field line y).

    Stack: sh4der_timebase_field keeps two arrays of one float a line on the
    stack, about 4.2 KB at the 480-row limit. Call it from a thread with that
    much room (the main thread; not a small worker stack, not an interrupt).

    One field depends on the one before only through the television loop's
    phase, kept in a small state block. Everything else is a pure function of
    the settings, the seed and the field's number.

    Maths: SH4ZAM through sh4der_core.h (sine, logarithm, exponential, square
    root, floor, multiply-add, divide); define SH4DER_CORE_NO_SH4ZAM for the
    plain C path. By hand: the 64-bit integer hash, the integer line counts
    and the noise positions (commented where they stand; a 64-bit division
    twice a field, none a line).

    Version: v0.1
    The effect: the sideways tearing, weaving and hooking of a picture played
      from video tape.
    Typical use: a tape look for a cutscene, a security monitor, a flashback, a
      broken broadcast, through a frame texture drawn as bands.

    \author Jilted Valkyrie
    \author Claude (an AI by Anthropic): the port and the tests
    \author Finn McKenty: the model (NTSCRT, MIT License, notice above)
*/

#ifndef SH4DER_TIMEBASE_H
#define SH4DER_TIMEBASE_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name The television signal's timing (NTSC) and the module's limits */
/**@{*/
#define SH4DER_TIMEBASE_LINE_US       63.556f   /**< \brief Length of one line, microseconds. */
#define SH4DER_TIMEBASE_ACTIVE_US     52.656f   /**< \brief The part of a line that carries picture, microseconds: the picture's width in time. */
#define SH4DER_TIMEBASE_FIELD_RATE_N  60000     /**< \brief Fields a second as a fraction: 60000 / 1001 (59.94). */
#define SH4DER_TIMEBASE_FIELD_RATE_D  1001
#define SH4DER_TIMEBASE_LINES_NTSC    262.5f    /**< \brief Lines of one field of the real signal. */
#define SH4DER_TIMEBASE_ROWS_MAX      480u      /**< \brief Most rows (active lines) the module simulates. */
#define SH4DER_TIMEBASE_VBI_MIN       12u       /**< \brief Fewest blanking lines above the picture a small raster gets. */
#define SH4DER_TIMEBASE_LINES_MAX     (SH4DER_TIMEBASE_ROWS_MAX + 40u)   /**< \brief Most lines a field then has (rows and blanking). */
#define SH4DER_TIMEBASE_HEAD_SWITCH_MAX_US 8.0f /**< \brief Largest head switching step, microseconds (the model's control range). */
#define SH4DER_TIMEBASE_FLUTTER_MAX   5.0f      /**< \brief Largest flutter amount (1 is a worn deck at its realistic worst). */
#define SH4DER_TIMEBASE_STRAIN        8.0e-4f   /**< \brief Tape stretch at tension 1: a 13 microsecond jump at the head switch. */
#define SH4DER_TIMEBASE_SEED_LO       0x53435254u /**< \brief The model's default seed, low and high word. */
#define SH4DER_TIMEBASE_SEED_HI       0x00004e54u
#define SH4DER_TIMEBASE_TAU           6.2831853f /**< \brief 2 pi. */
/**@}*/

/** \brief The deck and the set. Fill with sh4der_timebase_init, then change what you need. */
typedef struct sh4der_timebase {
    uint32_t rows;          /**< \brief Rows of the picture: active lines of the simulated raster (1 .. SH4DER_TIMEBASE_ROWS_MAX). The mesh's bands. */
    float width_px;         /**< \brief Width of the picture in pixels: a row's shift is given in these (> 0). */
    float amount;           /**< \brief Master amount, 0 .. 1: scales the three deck faults together (0: no error at all). */
    float head_switch_us;   /**< \brief Head switching step, microseconds (0 .. SH4DER_TIMEBASE_HEAD_SWITCH_MAX_US). */
    float flutter;          /**< \brief Drum flutter amount (0 .. SH4DER_TIMEBASE_FLUTTER_MAX). */
    float tension;          /**< \brief Back tension against the recording deck's, -1 .. 1: positive is tighter (the top of the picture hooks). */
    float afc_speed;        /**< \brief How fast the television's lock follows, 0 (slow) .. 1 (fast). */
    uint32_t line_noise;    /**< \brief 1: the flutter's line-by-line roughness is included (the model's own, a few hundredths of a pixel, and the
                                 costliest part: a logarithm and six hashes a line); 0: left out. */
    uint32_t seed_lo;       /**< \brief The 64-bit seed's low word: picks the wobble. */
    uint32_t seed_hi;       /**< \brief And its high word. */
} sh4der_timebase_t;

/** \brief What one field hands the next: the television loop's phase. Zero it (or call sh4der_timebase_reset) before the first field. */
typedef struct sh4der_timebase_state {
    float theta;            /**< \brief The set's line phase against the ideal raster, microseconds. */
} sh4der_timebase_state_t;

/* the model's 64-bit mixing hash (hand: integer arithmetic, exactly as the model's) */
static inline uint64_t sh4der_timebase_mix_(uint64_t z0) {
    uint64_t z = z0 + 0x9e3779b97f4a7c15ull;

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

/* the model's uniform(seed, purpose, a, 0): a number 0 .. 1 from a position, never from call order. The model keeps 53
 * bits; a float holds 24, so the top 24 are taken (hand: integer shift; the two agree to 6e-8). */
static inline float sh4der_timebase_uni_(uint64_t seed, uint64_t purpose, int64_t a) {
    const uint64_t h = sh4der_timebase_mix_(seed ^ sh4der_timebase_mix_(purpose ^ sh4der_timebase_mix_((uint64_t)a ^ sh4der_timebase_mix_(0ull))));

    return (float)(uint32_t)(h >> 40) * (1.0f / 16777216.0f);
}

/* the model's gauss(seed, purpose, a, 0): a standard normal number (Box-Muller) */
static inline float sh4der_timebase_gauss_(uint64_t seed, uint64_t purpose, int64_t a) {
    const float u1 = sh4der_timebase_uni_(seed, purpose, a);
    const float u2 = sh4der_timebase_uni_(seed, purpose + 0x51ull, a);
    float s, c;

    sh4der_sincos_(SH4DER_TIMEBASE_TAU * u2, &s, &c);
    (void)s;
    return sh4der_sqrt_(-2.0f * sh4der_ln_(u1 > 1.0e-12f ? u1 : 1.0e-12f)) * c;
}

/* Where a smooth noise stands on its lattice: the cell, how far into it, and the noise at the cell's two ends. The
 * position moves on by a fixed amount a line, so it is kept as a whole cell number and a whole remainder (in `den`-ths
 * of a cell) and walked by addition: a division only when it is first placed. */
typedef struct sh4der_timebase_cell_ {
    int64_t i;              /* the cell's number */
    int64_t rem;            /* the place inside the cell, 0 .. den - 1 */
    float a, b;             /* the noise at the cell's start and end, -1 .. 1; valid only when `have` is 1 */
    int have;
} sh4der_timebase_cell_t_;

/* Places a noise at position num / den (hand: 64-bit integer division, once a field; the floor, also for a negative
 * position). */
static inline void sh4der_timebase_seek_(sh4der_timebase_cell_t_ *cell, int64_t num, int64_t den) {
    cell->i = num / den;
    cell->rem = num - cell->i * den;
    if (cell->rem < 0) {
        cell->rem += den;
        cell->i--;
    }
    cell->have = 0;
}

/* Moves a noise on by add / den of a cell (add >= 0): an addition and a compare, no division. */
static inline void sh4der_timebase_step_(sh4der_timebase_cell_t_ *cell, int64_t add, int64_t den) {
    cell->rem += add;
    while (cell->rem >= den) {
        cell->rem -= den;
        cell->i++;
        cell->have = 0;
    }
}

/* the model's value(seed, purpose, x): smooth noise -1 .. 1 with a lattice point at every whole x, at the place `cell`
 * stands at. The two hashes are worked out once a cell, not once a line. */
static inline float sh4der_timebase_value_(uint64_t seed, uint64_t purpose, sh4der_timebase_cell_t_ *cell, int64_t den) {
    /* 0 .. 1 inside the lattice cell. Both numbers are under 2^31 (den is at most 520 lines x 60000), and are taken
     * through 32 bits: a 64-bit integer to float is a library call on the SH-4. */
    const float f = (float)(int32_t)cell->rem / (float)(int32_t)den;

    if (!cell->have) {
        cell->a = sh4der_timebase_uni_(seed, purpose, cell->i) * 2.0f - 1.0f;
        cell->b = sh4der_timebase_uni_(seed, purpose, cell->i + 1) * 2.0f - 1.0f;
        cell->have = 1;
    }
    return sh4der_fma_(cell->b - cell->a, f * f * (3.0f - 2.0f * f), cell->a);
}

/* What sh4der_timebase_error_ remembers from line to line of one field (sh4der_timebase_memo_zero_ before the first). */
typedef struct sh4der_timebase_memo_ {
    sh4der_timebase_cell_t_ hunt, rough;    /* the two smooth noises of the flutter */
    int64_t next_j;                         /* the line the two noises stand at; valid only when placed is 1 */
    int placed;
    int64_t wobble_field;                   /* the head switching step's wobble: its field, valid only when have_wobble is 1 */
    float wobble;                           /* and its value */
    int have_wobble;
    float phase;                            /* the drum wobble's phase, turns; valid only when have_phase is 1 */
    int have_phase;
} sh4der_timebase_memo_t_;

/* An empty memory: nothing placed, nothing worked out. */
static inline void sh4der_timebase_memo_zero_(sh4der_timebase_memo_t_ *m) {
    m->hunt.i = 0;
    m->hunt.rem = 0;
    m->hunt.a = 0.0f;
    m->hunt.b = 0.0f;
    m->hunt.have = 0;
    m->rough = m->hunt;
    m->next_j = 0;
    m->placed = 0;
    m->wobble_field = 0;
    m->wobble = 0.0f;
    m->have_wobble = 0;
    m->phase = 0.0f;
    m->have_phase = 0;
}

/** \brief Fill `p` with DEFAULTS for a picture of `rows` bands and `width_px` pixels: full amount, no head switching
    step, no flutter, matched tension, the lock at its middle speed, the model's seed. A deck in perfect order: every
    shift is 0 until a fault is turned up. */
static inline void sh4der_timebase_init(sh4der_timebase_t *p, uint32_t rows, float width_px) {
    if (!p) {
        return;
    }
    p->rows = rows;
    p->width_px = width_px;
    p->amount = 1.0f;
    p->head_switch_us = 0.0f;
    p->flutter = 0.0f;
    p->tension = 0.0f;
    p->afc_speed = 0.5f;
    p->line_noise = 1u;
    p->seed_lo = SH4DER_TIMEBASE_SEED_LO;
    p->seed_hi = SH4DER_TIMEBASE_SEED_HI;
}

/** \brief The loop's phase at rest (a set that has been locked to a perfect signal). */
static inline void sh4der_timebase_reset(sh4der_timebase_state_t *st) {
    if (st) {
        st->theta = 0.0f;
    }
}

/* 1 when v is finite and lo <= v <= hi */
static inline int sh4der_timebase_in_(float v, float lo, float hi) {
    return sh4der_finite_(v) && v >= lo && v <= hi;
}

/** \brief 1 when every field of `p` is finite and inside its stated range (see the struct), else 0. */
static inline int sh4der_timebase_valid(const sh4der_timebase_t *p) {
    if (!p || p->rows < 1u || p->rows > SH4DER_TIMEBASE_ROWS_MAX) {
        return 0;
    }
    return sh4der_timebase_in_(p->width_px, 1.0f, 4096.0f) && sh4der_timebase_in_(p->amount, 0.0f, 1.0f) &&
           sh4der_timebase_in_(p->head_switch_us, 0.0f, SH4DER_TIMEBASE_HEAD_SWITCH_MAX_US) &&
           sh4der_timebase_in_(p->flutter, 0.0f, SH4DER_TIMEBASE_FLUTTER_MAX) && sh4der_timebase_in_(p->tension, -1.0f, 1.0f) &&
           sh4der_timebase_in_(p->afc_speed, 0.0f, 1.0f) && p->line_noise <= 1u;
}

/** \brief Lines of blanking above the picture for a raster of `rows` active lines: in the real signal's proportion
    (20 to 242.5), at least SH4DER_TIMEBASE_VBI_MIN. */
static inline uint32_t sh4der_timebase_vbi(uint32_t rows) {
    const uint32_t v = (uint32_t)sh4der_floor_(sh4der_fma_((float)rows, 20.0f / 242.5f, 0.5f));   /* hand: rounded to a whole line */

    return v < SH4DER_TIMEBASE_VBI_MIN ? SH4DER_TIMEBASE_VBI_MIN : v;
}

/** \brief How many lines above the end of a field the heads switch, for a raster of `lines` lines a field (3.5 lines
    of the real signal, in the raster's proportion, rounded). The rows from there down are played by the next head. */
static inline uint32_t sh4der_timebase_switch_lead(uint32_t lines) {
    return (uint32_t)sh4der_floor_(sh4der_fma_(3.5f, (float)lines * (1.0f / SH4DER_TIMEBASE_LINES_NTSC), 0.5f));   /* hand: rounded */
}

/* The deck's time-base error of one signal line, microseconds: when the deck delivers the line against when it should.
 * The line is `line` of field `field` (`lines` a field; line may run a few lines before 0 and past the field's end:
 * those are the neighbouring fields' lines). The head switching step with its small wobble a field, the drum's flutter,
 * and the tension skew; all times the master amount. `m` carries what does not change from one line to the next;
 * called for lines in rising order it divides nothing after the first. */
static inline float sh4der_timebase_error_(const sh4der_timebase_t *p, int64_t field, int32_t line, uint32_t lines, uint32_t lead,
                                           sh4der_timebase_memo_t_ *m) {
    const uint64_t seed = ((uint64_t)p->seed_hi << 32) | (uint64_t)p->seed_lo;
    const int64_t l = (int64_t)lines;
    const int64_t j = field * l + (int64_t)line;        /* the line counted from the start of play (hand: one integer product) */
    int64_t f = field;                                  /* the field the line lies in */
    int64_t jf = (int64_t)line;                         /* and the line inside it, 0 .. lines - 1 */
    int switched;                                       /* 1: past the head switch, played by the next head */
    float e = 0.0f;

    while (jf < 0) {
        jf += l;
        f--;
    }
    while (jf >= l) {
        jf -= l;
        f++;
    }
    switched = jf >= l - (int64_t)lead;
    if (p->head_switch_us > 0.0f) {
        const int64_t fs = switched ? f + 1 : f;        /* the field whose head plays it */

        if (!m->have_wobble || m->wobble_field != fs) {
            m->wobble_field = fs;
            m->wobble = sh4der_fma_(0.15f, sh4der_timebase_gauss_(seed, 31ull, fs), 1.0f);
            m->have_wobble = 1;
        }
        e += ((fs & 1) == 0 ? 0.5f : -0.5f) * p->head_switch_us * m->wobble;
    }
    if (p->flutter > 0.0f) {
        /* The drum turns once in two fields: its wobble is a sine of the line's place in that turn. The slower hunting
         * and the roughness are smooth noise at 4 and at 180 lattice points a second: positions j x 4 / (lines x field
         * rate) and j x 180 / (lines x field rate), kept as whole-number fractions and walked a line at a time. */
        const int64_t turn2 = ((f & 1) != 0 ? l : 0) + jf;   /* the line inside the drum's turn, 0 .. 2 x lines - 1 */
        const int64_t den = l * SH4DER_TIMEBASE_FIELD_RATE_N;
        float s, c;

        if (!m->have_phase) {
            m->phase = sh4der_timebase_uni_(seed, 40ull, 0);
            m->have_phase = 1;
        }
        if (!m->placed || m->next_j != j) {
            sh4der_timebase_seek_(&m->hunt, j * 4 * SH4DER_TIMEBASE_FIELD_RATE_D, den);
            sh4der_timebase_seek_(&m->rough, j * 180 * SH4DER_TIMEBASE_FIELD_RATE_D, den);
            m->placed = 1;
        }
        sh4der_sincos_(SH4DER_TIMEBASE_TAU * sh4der_frac_((float)turn2 / (float)(2 * l) + m->phase), &s, &c);
        (void)c;
        e += p->flutter * (0.8f * s + 1.3f * sh4der_timebase_value_(seed, 41ull, &m->hunt, den) +
                           0.45f * sh4der_timebase_value_(seed, 42ull, &m->rough, den) +
                           (p->line_noise ? 0.07f * sh4der_timebase_gauss_(seed, 43ull, j) : 0.0f));
        sh4der_timebase_step_(&m->hunt, 4 * SH4DER_TIMEBASE_FIELD_RATE_D, den);     /* on to the next line */
        sh4der_timebase_step_(&m->rough, 180 * SH4DER_TIMEBASE_FIELD_RATE_D, den);
        m->next_j = j + 1;
    }
    if (p->tension != 0.0f) {
        /* Stretched further than when recorded, the lines arrive later the further down the track; the next head starts
         * at its own track's start and the error jumps back. A whole field of track is the real signal's 262.5 lines long. */
        const float line_in_track = (float)(switched ? jf - l : jf);

        e += SH4DER_TIMEBASE_STRAIN * p->tension * line_in_track * (SH4DER_TIMEBASE_LINES_NTSC / (float)lines) * SH4DER_TIMEBASE_LINE_US;
    }
    return e * p->amount;
}

/** \brief One field: fills shift_px[0 .. rows - 1] with each row's sideways shift in pixels (positive: the row's
    picture sits to the right) and moves the television loop on in `st`. `field` counts fields from the start of play
    (call with 0, 1, 2 ... for a running picture; any other order still gives a valid picture, with the loop settling
    again). Returns the number of rows written: p->rows, or 0 for an invalid block or a missing pointer (then nothing is
    written and the state is untouched).
    How: the deck's error of every line of the field is worked out; the set scans the field line by line, each time
    comparing where it is with where the nearest line of the signal began and steering toward it by its loop gain
    (the model's sinusoidal phase detector, proportional only); a row's shift is how far the set's line start then lies
    from the start of the signal line it shows. */
static inline uint32_t sh4der_timebase_field(const sh4der_timebase_t *p, sh4der_timebase_state_t *st, uint32_t field, float *shift_px) {
    const float h = SH4DER_TIMEBASE_LINE_US;
    float err[SH4DER_TIMEBASE_LINES_MAX + 4u];          /* the deck's error of lines -2 .. lines + 1 of this field */
    float theta_at[SH4DER_TIMEBASE_LINES_MAX];          /* the set's phase at the start of each of its lines */
    sh4der_timebase_memo_t_ memo;
    uint32_t vbi, lines, lead, r, y;
    float scale, kp, theta, px_per_us;

    if (!sh4der_timebase_valid(p) || !st || !shift_px || !sh4der_finite_(st->theta)) {
        return 0;
    }
    vbi = sh4der_timebase_vbi(p->rows);
    lines = p->rows + vbi;
    if (vbi > SH4DER_TIMEBASE_LINES_MAX - SH4DER_TIMEBASE_ROWS_MAX || lines > SH4DER_TIMEBASE_LINES_MAX) {
        return 0;                                       /* cannot happen for valid rows; keeps every index below in its array */
    }
    lead = sh4der_timebase_switch_lead(lines);
    scale = (float)lines * (1.0f / SH4DER_TIMEBASE_LINES_NTSC);          /* raster lines per line of the real signal */
    kp = sh4der_div_(0.04f * sh4der_exp_(2.3025851f * p->afc_speed), scale);   /* the loop gain: 0.04 x 10^speed, a real line */
    px_per_us = p->width_px * (1.0f / SH4DER_TIMEBASE_ACTIVE_US);
    sh4der_timebase_memo_zero_(&memo);
    for (r = 0; r < lines + 4u; r++) {
        err[r] = sh4der_timebase_error_(p, (int64_t)field, (int32_t)r - 2, lines, lead, &memo);
    }
    theta = sh4der_clamp_(st->theta, -0.5f * h, 0.5f * h);              /* a phase is never more than half a line out */
    for (r = 0; r < lines; r++) {
        /* The signal line whose start is nearest this scanned line's start: line r itself, or a neighbour when the phase
         * and the errors are large. d is how far ahead of the set that start lies. */
        int32_t j = (int32_t)r;
        float d = err[j + 2] - theta;                   /* (j x h + error) - (r x h + theta), with j = r */
        float s, c;

        if (d < -0.5f * h) {
            j++;
            d = h + err[j + 2] - theta;
        } else if (d > 0.5f * h) {
            j--;
            d = err[j + 2] - h - theta;
        }
        theta_at[r] = theta;
        sh4der_sincos_(SH4DER_TIMEBASE_TAU * d * (1.0f / h), &s, &c);
        (void)c;
        theta = sh4der_fma_(kp * (h * (1.0f / SH4DER_TIMEBASE_TAU)), s, theta);   /* steer by the gain times the detector's reading */
    }
    st->theta = theta;
    for (y = 0; y < p->rows; y++) {
        /* Row y is scanned line vbi + y. u is how far into its signal line that scan starts, wrapped to half a line
         * either way: a scan that starts late in the line shows the picture pushed left. */
        r = vbi + y;
        {
            float u = theta_at[r] - err[r + 2u];

            if (u > 0.5f * h) {
                u -= h;
            } else if (u < -0.5f * h) {
                u += h;
            }
            shift_px[y] = -u * px_per_us;
        }
    }
    return p->rows;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_TIMEBASE_H */
