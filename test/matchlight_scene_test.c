/* SH4DER-PVR: match girl film scene test

   test/matchlight_scene_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of demo/matchlight/matchlight_scene.h (the plain C path, -DSH4DER_CORE_NO_SH4ZAM): the timeline covers
   0 to 120 s with no gap or overlap and loops; the match shots are the ones the film spec names; every camera looks at a
   finite target, keeps the girl 6 m or more from the lens and keeps her middle inside the 640 x 480 frame all through its
   move; the set tables hold the counts the README claims; the fog, the snow light, the sprite atlas, the footprints and
   the cut overlay behave as documented; no house and no snow drift stands between any camera and its subject (the
   per-shot table is printed); the snow is shaped and level on the girl's path; the footprint pits fit the trail patch
   and their size on screen is printed. Prints the counted triangle budget of each shot.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../demo/matchlight/matchlight_scene.h"

/* Failure counter and the check macro every test in this folder uses. */
static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* A screen y as a percentage down the 640 x 360 middle band of the 640 x 480 frame: the frame the staging table's
 * percentages are measured in. */
#define BAND_PCT(y) (100.0f * ((y) - 0.5f * (float)(ML_SCREEN_H - ML_BAND_H)) / (float)ML_BAND_H)

/* Progress samples taken along each shot when its camera is checked: start, middle and end of any camera move, and
 * points between, so the crane and the pulls are covered. */
#define PROGRESS_STEPS 20

/* The timeline: the twelve shots tile 0 .. 120 s with no gap or overlap, every time maps to a shot in order, and it loops. */
static void timeline(void) {
    ml_where_t w = { 0 }, prev = { 0 };
    int k, last_shot = 0;
    float t;

    CHECK(ML_SHOTS == 12 && ML_FILM_SECONDS == 120.0f && ML_SHOT_M10 == ML_SHOTS - 1);
    CHECK(ml_shots[0].start_s == 0.0f);
    CHECK(ml_shots[ML_SHOTS - 1].end_s == ML_FILM_SECONDS);
    /* The end credits follow the film: 9 cards, 32 s; the clock wraps after both. */
    {
        ml_where_t w = { 0 };
        unsigned k;
        float s;

        CHECK(ML_CREDIT_CARDS == 9u && fabsf(ML_CREDITS_SECONDS - 32.0f) < 1.0e-4f && fabsf(ML_LOOP_SECONDS - 159.0f) < 1.0e-4f);
        /* The title card is the loop's last stretch: 7 s, over the first shot's first frame, up from and down to black. */
        CHECK(fabsf(ML_TITLE_START_S - 152.0f) < 1.0e-4f && ML_TITLE_SECONDS == 7.0f);
        ml_locate(ML_TITLE_START_S + 2.0f, &w);
        CHECK(w.shot == ML_SHOT_M1 && w.seconds == 0.0f && w.progress == 0.0f && fabsf(w.title_s - 2.0f) < 1.0e-3f && w.credits_s < 0.0f);
        ml_locate(60.0f, &w);
        CHECK(w.title_s < 0.0f);
        ml_locate(ML_FILM_SECONDS + 5.0f, &w);
        CHECK(w.title_s < 0.0f);
        CHECK(ml_title_alpha(0.0f) == 0.0f && ml_title_alpha(0.5f * ML_TITLE_IN_S) > 0.4f && ml_title_alpha(3.0f) == 1.0f);
        CHECK(ml_title_alpha(ML_TITLE_SECONDS - 0.5f * ML_TITLE_OUT_S) < 0.6f && ml_title_alpha(ML_TITLE_SECONDS) == 0.0f && ml_title_alpha(-1.0f) == 0.0f);
        ml_locate(60.0f, &w);
        CHECK(w.credits_s < 0.0f);
        ml_locate(ML_FILM_SECONDS + 5.0f, &w);
        CHECK(w.shot == ML_SHOTS - 1 && fabsf(w.credits_s - 5.0f) < 1.0e-3f && w.progress == 1.0f);
        ml_locate(ML_LOOP_SECONDS + 1.0f, &w);
        CHECK(w.shot == ML_SHOT_M1 && fabsf(w.clock - 1.0f) < 1.0e-3f && w.credits_s < 0.0f);
        CHECK(ml_credit_alpha(0u, 0.0f) == 0.0f && ml_credit_alpha(0u, 0.5f) > 0.4f && ml_credit_alpha(0u, 0.5f) < 0.6f && ml_credit_alpha(0u, 2.0f) == 1.0f);
        CHECK(ml_credit_alpha(ML_CREDIT_CARDS - 1u, ML_CREDITS_SECONDS - 1.1f) == 1.0f && ml_credit_alpha(ML_CREDIT_CARDS - 1u, ML_CREDITS_SECONDS - 0.5f) < 0.6f);
        CHECK(ml_credit_alpha(ML_CREDIT_CARDS - 1u, ML_CREDITS_SECONDS) < 1.0e-3f && ml_credit_alpha(ML_CREDIT_CARDS, 10.0f) == 0.0f);
        for (s = 0.0f; s < ML_CREDITS_SECONDS; s += 0.05f) {      /* at every moment: one card, or two that dissolve and sum to one */
            float sum = 0.0f;
            unsigned showing = 0u;

            for (k = 0u; k < ML_CREDIT_CARDS; k++) {
                const float a = ml_credit_alpha(k, s);

                CHECK(a >= 0.0f && a <= 1.0f);
                sum += a;
                showing += a > 0.0f ? 1u : 0u;
            }
            CHECK(showing <= 2u);
            if (s >= ML_CREDIT_IN_S && s <= ML_CREDITS_SECONDS - ML_CREDIT_OUT_S) {
                CHECK(fabsf(sum - 1.0f) < 1.0e-3f && ml_credit_frame_alpha(s) == 1.0f);
            }
        }
        for (k = 0u; k < ML_CREDIT_CARDS; k++) {                 /* every card is held 3 s */
            const float start = (float)k * (ML_CREDIT_HOLD_S + ML_CREDIT_DISSOLVE_S);

            CHECK(ml_credit_alpha(k, start + (k == 0u ? ML_CREDIT_IN_S : 0.0f) + 0.01f) == 1.0f && ml_credit_alpha(k, start + ML_CREDIT_HOLD_S - 0.01f) == 1.0f);
        }
    }
    for (k = 0; k + 1 < ML_SHOTS; k++) {
        CHECK(ml_shots[k].end_s == ml_shots[k + 1].start_s);   /* no gap, no overlap */
        CHECK(ml_shots[k].end_s > ml_shots[k].start_s);
    }
    ml_locate(0.0f, &prev);
    for (t = 0.0f; t < ML_FILM_SECONDS; t += 0.01f) {
        ml_locate(t, &w);
        CHECK(w.shot >= last_shot && w.shot <= last_shot + 1);   /* shots come in order, none skipped */
        CHECK(w.progress >= 0.0f && w.progress <= 1.0f);
        CHECK(t >= ml_shots[w.shot].start_s - 0.001f && t < ml_shots[w.shot].end_s + 0.001f);
        last_shot = w.shot;
    }
    CHECK(last_shot == ML_SHOTS - 1);
    /* Looping: the end of the loop (film and credits) is the start again, and so is any whole number of loops; a
     * negative time wraps upward, into the title card. */
    ml_locate(ML_LOOP_SECONDS, &w);
    CHECK(w.shot == 0 && fabsf(w.clock) < 0.001f);
    ml_locate(ML_LOOP_SECONDS + 3.5f, &w);
    CHECK(w.shot == 0 && fabsf(w.clock - 3.5f) < 0.001f);
    ml_locate(-1.0f, &w);
    CHECK(w.shot == ML_SHOT_M1 && fabsf(w.clock - (ML_LOOP_SECONDS - 1.0f)) < 0.001f && w.title_s > 0.0f);   /* into the title card */
    ml_locate(32.0f, &w);                                       /* a boundary belongs to the shot that starts there */
    CHECK(w.shot == ML_SHOT_M3 && w.progress < 0.001f);
    ml_locate(119.999f, &w);
    CHECK(w.shot == ML_SHOT_M10 && w.progress > 0.99f);
}

/* The match state: lit in M3, M5, M6, M8, M9; not in M1, M2, M2b, M4b, M7, M10; in M4 only the first 0.4 s (it goes out).
 * A struck match is struck on the cut: its shot opens on the flash. */
static void match_state(void) {
    static const int lit_shots[ML_SHOTS] = { 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 };
    int k;

    for (k = 0; k < ML_SHOTS; k++) {
        CHECK(ml_shot_lit(k) == lit_shots[k]);
        if (!lit_shots[k]) {
            CHECK(ml_match_level(k, 1.0f) == 0.0f);
            CHECK(ml_shots[k].flash == 0);
        } else if (k != ML_SHOT_M4) {
            CHECK(ml_shots[k].flash == 1 && ml_shots[k].match_on_s == 0.0f && ml_shots[k].in_s == 0.0f);
            CHECK(ml_shots[k - 1].out_s == 0.0f);               /* the cut into a strike is hard */
            CHECK(ml_match_level(k, ml_shots[k].end_s - ml_shots[k].start_s - 0.01f) == 1.0f);   /* it burns to the end of its shot */
        }
    }
    CHECK(ml_match_level(ML_SHOT_M3, 0.05f) > 0.0f && ml_match_level(ML_SHOT_M3, 0.05f) < 1.0f);   /* M3: rising */
    CHECK(ml_match_level(ML_SHOT_M3, 4.0f) == 1.0f);            /* M3: held */
    CHECK(ml_match_level(ML_SHOT_M4, 0.0f) == 1.0f);            /* M4 opens on the last of the flame */
    CHECK(ml_match_level(ML_SHOT_M4, 0.2f) > 0.0f && ml_match_level(ML_SHOT_M4, 0.2f) < 1.0f);
    CHECK(ml_match_level(ML_SHOT_M4, 0.45f) == 0.0f);           /* and it is out within 0.4 s */
    CHECK(ml_shots[ML_SHOT_M9].pool_radius >= 2.5f);            /* M9 is the largest pool */
    CHECK(ml_shots[ML_SHOT_M9].pool_radius > 1.5f * ml_shots[ML_SHOT_M3].pool_radius);   /* and reads larger than M3 by at least 1.5 x */
    CHECK(ml_shots[ML_SHOT_M6].has_tree == 1 && ml_shots[ML_SHOT_M3].has_tree == 0);
}

/* Cameras: finite targets, the girl 6 m or more away, the girl's middle on screen, all along every move. */
static void cameras(void) {
    int k, i;
    float worst_dist = 1.0e9f;

    for (k = 0; k < ML_SHOTS; k++) {
        for (i = 0; i <= PROGRESS_STEPS; i++) {
            const float p = (float)i / (float)PROGRESS_STEPS;
            ml_camera_t c = { 0 };
            float girl[3], centre[3], d[3], dist;
            ml_screen_t s = { 0.0f, 0.0f, 0.0f };
            int j;

            CHECK(ml_camera_at(k, p, &c) == 1);
            for (j = 0; j < 3; j++) {
                CHECK(sh4der_finite_(c.target[j]) && sh4der_finite_(c.eye[j]));
            }
            CHECK(fabsf(sh4der_len3_(c.fwd[0], c.fwd[1], c.fwd[2]) - 1.0f) < 1.0e-4f);   /* unit axes */
            CHECK(fabsf(sh4der_len3_(c.right[0], c.right[1], c.right[2]) - 1.0f) < 1.0e-4f);
            CHECK(fabsf(sh4der_dot3_(c.right, c.fwd)) < 1.0e-4f);
            ml_girl_pos(k, p, girl);
            ml_girl_centre(k, p, centre);
            d[0] = girl[0] - c.eye[0]; d[1] = girl[1] - c.eye[1]; d[2] = girl[2] - c.eye[2];
            dist = sqrtf(sh4der_len3_(d[0], d[1], d[2]));
            if (dist < worst_dist) {
                worst_dist = dist;
            }
            CHECK(dist >= 6.0f);
            CHECK(ml_project(&c, centre, &s) == 1);
            if (k == ML_SHOT_M7) {
                continue;                                        /* the sky shot: the lens is tilted up over her */
            }
            CHECK(ml_in_frame(&s));
            if (!ml_in_frame(&s)) {
                fprintf(stderr, "  shot %s progress %.2f: girl at screen (%.1f, %.1f)\n", ml_shots[k].name, (double)p, (double)s.x, (double)s.y);
            }
        }
    }
    printf("  nearest the girl ever is to a lens: %.2f m (counted from the tables)\n", (double)worst_dist);
}

/* The projection itself: the look-at point lands in the middle of the frame, a point behind the near plane is refused,
 * and the field of view is the horizontal one (the point at half the field to the right lands on the right edge). */
static void projection(void) {
    ml_camera_t c = { 0 };
    ml_screen_t s = { 0.0f, 0.0f, 0.0f };
    float behind[3], edge[3], half_s, half_c, reach;

    CHECK(ml_camera_at(0, 0.0f, &c) == 1);
    CHECK(ml_project(&c, c.target, &s) == 1);
    CHECK(fabsf(s.x - 320.0f) < 0.01f && fabsf(s.y - 240.0f) < 0.01f);
    behind[0] = c.eye[0]; behind[1] = c.eye[1]; behind[2] = c.eye[2] - 1.0f;
    CHECK(ml_project(&c, behind, &s) == 0);
    sh4der_sincos_(30.0f * ML_DEG2RAD, &half_s, &half_c);       /* M1 is 60 degrees: half is 30 */
    reach = 10.0f * half_s / half_c;                            /* x at depth 10 m seen at the half angle */
    edge[0] = c.eye[0] + reach; edge[1] = c.eye[1]; edge[2] = c.eye[2] + 10.0f;
    CHECK(ml_project(&c, edge, &s) == 1);
    CHECK(fabsf(s.x - 640.0f) < 1.0f);
    CHECK(c.focal_px > 500.0f && c.focal_px < 600.0f);          /* 320 / tan(30 degrees) = 554.3 */
    CHECK(ml_camera_at(-1, 0.0f, &c) == 0 && ml_camera_at(ML_SHOTS, 0.0f, &c) == 0);   /* bad index */
    /* M10: the pull ends 9 s into the 12 s shot and then holds; she lies about 70 % down the frame at the start (6 m) and
     * 64 % at the end (9 m) of the 16:9 picture, in its middle across; M9: her ground is about 75 % down the frame. */
    {
        float g[3];
        ml_camera_t held = { 0 };

        ml_girl_centre(ML_SHOT_M10, 0.0f, g);
        CHECK(ml_camera_at(ML_SHOT_M10, 0.0f, &c) == 1 && c.hfov_deg == 60.0f && fabsf(c.eye[1] - 0.8f) < 1.0e-4f);
        CHECK(ml_project(&c, g, &s) == 1 && fabsf(s.x - 320.0f) < 8.0f && fabsf(BAND_PCT(s.y) - 70.0f) < 1.5f);
        CHECK(ml_camera_at(ML_SHOT_M10, 0.75f, &held) == 1 && ml_camera_at(ML_SHOT_M10, 1.0f, &c) == 1);
        CHECK(fabsf(held.eye[0] - c.eye[0]) < 1.0e-4f && fabsf(held.eye[2] - c.eye[2]) < 1.0e-4f);   /* the hold */
        CHECK(ml_project(&c, g, &s) == 1 && fabsf(s.x - 320.0f) < 8.0f && fabsf(BAND_PCT(s.y) - 64.0f) < 1.5f);
        CHECK(ml_camera_at(ML_SHOT_M10, 0.375f, &c) == 1 && c.eye[2] < 9.45f && c.eye[2] > 6.68f);   /* half way through the pull */
        /* M9, measured in the 640 x 360 middle band (the staging table's frame): ground 75 to 76 % down, seated head top 58 to 59 % */
        g[1] = 0.0f;
        CHECK(ml_camera_at(ML_SHOT_M9, 0.5f, &c) == 1 && fabsf(c.fwd[1]) < 1.0e-4f && ml_project(&c, g, &s) == 1);   /* a level lens */
        CHECK(BAND_PCT(s.y) > 74.5f && BAND_PCT(s.y) < 76.5f);
        g[1] = ML_GIRL_SEATED_HEIGHT;
        CHECK(ml_project(&c, g, &s) == 1 && BAND_PCT(s.y) > 57.5f && BAND_PCT(s.y) < 59.5f);
    }
    /* The M6 crane really moves up, with an ease: the middle is half way, the ends are the poses. */
    CHECK(ml_camera_at(ML_SHOT_M6, 0.0f, &c) == 1 && fabsf(c.eye[1] - 1.0f) < 1.0e-4f);
    CHECK(ml_camera_at(ML_SHOT_M6, 0.5f, &c) == 1 && fabsf(c.eye[1] - 1.7f) < 1.0e-4f);
    CHECK(ml_camera_at(ML_SHOT_M6, 1.0f, &c) == 1 && fabsf(c.eye[1] - 2.4f) < 1.0e-4f);
}

/* The set tables hold the counts the README claims. */
static void set_tables(void) {
    int left = 0, right = 0, tall = 0;
    unsigned i, j, lit_total = 0;

    CHECK(ML_HOUSES == 10);
    for (i = 0; i < ML_HOUSES; i++) {
        const ml_house_t *h = &ml_houses[i];
        float lo, hi;

        if (h->face_x < 0.0f) {
            left++;
        } else {
            right++;
        }
        CHECK(h->width > 3.0f && h->height > 4.0f && h->gable > 1.0f && h->depth > 0.0f);
        CHECK(h->win_cols >= 1 && h->win_rows >= 1);
        CHECK(h->lit_mask != 0u);                                /* every front has at least one pale window */
        CHECK(h->z0 + h->width <= ML_GROUND_Z0 + (float)ML_GROUND_ROWS * ML_GROUND_DZ);   /* the front ends on the ground grid */
        CHECK(fabsf(h->face_x) + h->depth <= -ML_GROUND_X0);     /* and so does its box */
        for (j = 0; j < (unsigned)h->win_cols * h->win_rows; j++) {
            ml_quad_t q = { { { 0.0f } } };
            int lit = -1;

            CHECK(ml_house_window(h, j, &q, &lit) == 1);
            lit_total += (unsigned)lit;
            lo = q.p[0][2]; hi = q.p[2][2];
            CHECK(lo >= h->z0 && hi <= h->z0 + h->width);        /* inside the wall along the street */
            CHECK(q.p[1][1] < h->height);                        /* under the eaves */
        }
        {
            ml_quad_t q = { 0 };
            int lit;

            CHECK(ml_house_window(h, (unsigned)h->win_cols * h->win_rows, &q, &lit) == 0);   /* one past the grid */
        }
        if (h->height + h->gable > 7.5f) {
            tall++;
        }
    }
    CHECK(left == 5 && right == 5);
    CHECK(tall >= 2);                                           /* the heights vary */
    CHECK(lit_total == ml_count_windows(1));
    CHECK(ml_count_windows(0) > ml_count_windows(1));
    CHECK(ML_GROUND_COLS == 25 && ML_GROUND_ROWS == 40 && ML_GROUND_VERTS == 26 * 41);
    CHECK(ML_GROUND_COLS * ML_GROUND_DX == 25.0f && ML_GROUND_ROWS * ML_GROUND_DZ == 40.0f);   /* a 40 m ground */
    CHECK(ML_GIRL_QUADS == 3 && ML_GIRL_SEATED_QUADS == 4 && ML_TREE_CANDLES == 48u);
    for (i = 0; i < ML_TREE_CANDLES; i++) {               /* every candle is on the tree: inside the tiers' outline, under the top */
        float across, up;

        ml_tree_candle((unsigned)i, &across, &up);
        CHECK(up > ml_tree_tier[0][1] && up < ML_TREE_HEIGHT);
        CHECK(fabsf(across) < ml_tree_tier[0][0] * (1.0f - up / ML_TREE_HEIGHT) + 0.2f);
    }
    CHECK(ML_TREE_HEIGHT >= 5.0f * ML_GIRL_SEATED_HEIGHT && ml_tree_tier[2][2] == ML_TREE_HEIGHT);
    CHECK(ML_TREE_CANDLES >= 40u && ML_TREE_CANDLES <= 60u && ML_TREE_ORNAMENTS == 3u);
    /* The tale's props. */
    {
        ml_camera_t c = { 0 };
        ml_screen_t a = { 0 }, b = { 0 };
        const float band0 = 0.5f * (float)(ML_SCREEN_H - ML_BAND_H);
        float p0[3] = { ML_SLIPPER_X - 0.13f, 0.03f, ML_SLIPPER_Z }, p1[3] = { ML_SLIPPER_X + 0.13f, 0.03f, ML_SLIPPER_Z };
        float nearer0, up0, nearer1, up1, top[3] = { ML_TREE_X, ML_TREE_HEIGHT, ML_TREE_Z };
        unsigned n = 0u, k;

        /* M2b: the slipper is in the lower left of the picture and at least 12 pixels long */
        CHECK(ml_camera_at(ML_SHOT_M2B, 0.5f, &c) == 1 && ml_project(&c, p0, &a) && ml_project(&c, p1, &b));
        CHECK(ml_in_frame(&a) && ml_in_frame(&b) && fabsf(b.x - a.x) >= 12.0f);
        CHECK(b.x < 0.5f * (float)ML_SCREEN_W && a.y > 0.5f * (float)ML_SCREEN_H && a.y < band0 + (float)ML_BAND_H);
        printf("  slipper (M2b): %.0f pixels long at x %.0f .. %.0f, y %.0f\n", (double)fabsf(b.x - a.x), (double)a.x, (double)b.x, (double)a.y);
        /* M3 stove with six brass parts, M5 table, M8 grandmother; no prop elsewhere */
        CHECK(ml_vision_parts(ML_SHOT_M3, &n) == ml_stove_parts && n == ML_STOVE_PARTS && ML_STOVE_BRASS == 6u);
        CHECK(ml_vision_parts(ML_SHOT_M5, &n) == ml_table_parts && ml_vision_parts(ML_SHOT_M8, &n) == ml_grandmother_parts);
        CHECK(ml_vision_parts(ML_SHOT_M6, &n) == (const ml_part_t *)0 && n == 0u);
        /* M5: the goose lies on its dish, then comes at least 1 m toward the lens in the last 2 s before the match is out */
        ml_goose_place(0.0f, &nearer0, &up0);
        CHECK(nearer0 == 0.0f && up0 == ML_GOOSE_DISH_Y);
        ml_goose_place(ml_shots[ML_SHOT_M5].match_off_s - 2.0f, &nearer0, &up0);
        ml_goose_place(ml_shots[ML_SHOT_M5].match_off_s, &nearer1, &up1);
        CHECK(nearer1 - nearer0 >= 1.0f && up1 < 0.05f);
        /* M6: the tree's top and the seated girl are both in the picture's band, at both ends of the crane */
        for (k = 0u; k <= 1u; k++) {
            float girl[3];

            CHECK(ml_camera_at(ML_SHOT_M6, (float)k, &c) == 1 && ml_project(&c, top, &a));
            ml_girl_pos(ML_SHOT_M6, (float)k, girl);
            CHECK(ml_project(&c, girl, &b));
            CHECK(a.y > band0 && b.y < band0 + (float)ML_BAND_H && a.x > 0.0f && a.x < (float)ML_SCREEN_W);
            printf("  tree (M6, crane %u): top at y %.0f, x %.0f; her feet at y %.0f (band %.0f .. %.0f)\n", k, (double)a.y, (double)a.x, (double)b.y, (double)band0, (double)(band0 + (float)ML_BAND_H));
        }
        /* M7: the streak (head to the end of its tail) spans at least 40 % of the frame's width */
        {
            const float head = 0.82f, tail = head - ML_FALL_TAIL_GAP * (float)ML_FALL_TAIL;
            float ph[3], pt[3];
            int j;

            for (j = 0; j < 3; j++) {
                ph[j] = ml_fall_from[j] + (ml_fall_to[j] - ml_fall_from[j]) * head;
                pt[j] = ml_fall_from[j] + (ml_fall_to[j] - ml_fall_from[j]) * tail;
            }
            CHECK(ml_camera_at(ML_SHOT_M7, 0.5f, &c) == 1 && ml_project(&c, ph, &a) && ml_project(&c, pt, &b));
            CHECK(ml_in_frame(&a) && ml_in_frame(&b) && a.y > band0 && b.y > band0);
            CHECK(fabsf(a.x - b.x) >= 0.40f * (float)ML_SCREEN_W);
            printf("  falling star (M7): streak %.0f pixels = %.0f %% of the width\n", (double)fabsf(a.x - b.x), (double)(fabsf(a.x - b.x) * 100.0f / (float)ML_SCREEN_W));
        }
        /* M8 lifts the whole frame; M9's peak is more than twice that and holds 1 s; no lift elsewhere */
        CHECK(ml_lift_level(ML_SHOT_M8, 4.0f) > 0.0f && ml_lift_level(ML_SHOT_M3, 4.0f) == 0.0f && ml_lift_level(ML_SHOT_M10, 4.0f) == 0.0f);
        CHECK(ml_lift_level(ML_SHOT_M9, 0.0f) >= 2.0f * ml_lift_level(ML_SHOT_M8, 4.0f));
        CHECK(ml_lift_level(ML_SHOT_M9, 0.95f) >= 2.0f * ml_lift_level(ML_SHOT_M8, 4.0f));
        CHECK(ml_lift_level(ML_SHOT_M9, 3.0f) < ml_lift_level(ML_SHOT_M9, 0.5f) && ml_lift_level(ML_SHOT_M9, 3.0f) > ml_lift_level(ML_SHOT_M8, 4.0f));
        /* the walk: stooped all through M2, never elsewhere */
        CHECK(ml_girl_walk_drop(ML_SHOT_M2, 0.3f) >= ML_WALK_STOOP && ml_girl_walk_drop(ML_SHOT_M2B, 0.3f) == 0.0f);
        CHECK(ML_STUB_PARTS >= 2u && ML_STUB_PARTS <= 4u);
    }
    printf("  houses %d left + %d right, windows %u (%u lit)\n", left, right, ml_count_windows(0), ml_count_windows(1));
}

/* The girl: about 1.1 m tall, every quad has area, and she is turned to the camera. */
static void girl(void) {
    ml_camera_t c = { 0 };
    ml_quad_t q[ML_GIRL_QUADS] = { 0 };
    float pos[3] = { 0.0f, 0.0f, ML_GIRL_STAND_Z };
    float top = 0.0f, bottom = 1.0e9f;
    int i, k;

    CHECK(ml_camera_at(ML_SHOT_M3, 0.0f, &c) == 1);
    ml_girl_build(&c, pos, q);
    for (i = 0; i < ML_GIRL_QUADS; i++) {
        float ex, ey, ez, fx, fy, fz, cx, cy, cz;

        for (k = 0; k < 4; k++) {
            if (q[i].p[k][1] > top) {
                top = q[i].p[k][1];
            }
            if (q[i].p[k][1] < bottom) {
                bottom = q[i].p[k][1];
            }
        }
        ex = q[i].p[1][0] - q[i].p[0][0]; ey = q[i].p[1][1] - q[i].p[0][1]; ez = q[i].p[1][2] - q[i].p[0][2];
        fx = q[i].p[2][0] - q[i].p[0][0]; fy = q[i].p[2][1] - q[i].p[0][1]; fz = q[i].p[2][2] - q[i].p[0][2];
        cx = ey * fz - ez * fy; cy = ez * fx - ex * fz; cz = ex * fy - ey * fx;
        CHECK(sh4der_len3_(cx, cy, cz) > 1.0e-6f);              /* the quad has area */
    }
    CHECK(top < ML_GIRL_HEIGHT && fabsf(bottom) < 0.001f);
    /* The round head of both poses: its top is the pose's height, every rim point is one radius from the centre. */
    for (i = ML_POSE_STAND; i <= ML_POSE_LYING; i++) {
        float hp[ML_GIRL_HEAD_SIDES + 1u][3];
        float h_top = 0.0f;

        ml_girl_head(&c, pos, i, hp);
        for (k = 1; k <= (int)ML_GIRL_HEAD_SIDES; k++) {
            const float d = sqrtf(sh4der_len3_(hp[k][0] - hp[0][0], hp[k][1] - hp[0][1], hp[k][2] - hp[0][2]));

            CHECK(fabsf(d - ML_GIRL_HEAD_RADIUS) < 1.0e-4f);
            h_top = fmaxf(h_top, hp[k][1]);
        }
        CHECK(fabsf(h_top - ml_girl_pose_height(i)) < 0.001f);
    }
    /* The seated pose: four quads with area, on the snow, lower than she stands, no wider than 0.8 m. */
    {
        ml_quad_t sq[ML_GIRL_SEATED_QUADS] = { 0 };
        float s_top = 0.0f, s_bottom = 1.0e9f;

        for (k = 0; k < ML_SHOTS; k++) {
            CHECK(ml_girl_seated(k) == (k >= ML_SHOT_M4B && k <= ML_SHOT_M9));   /* seated M4b .. M9: her body does not rise */
            CHECK((ml_girl_pose(k) == ML_POSE_LYING) == (k == ML_SHOT_M10));     /* the morning: she lies in the snow */
            CHECK((ml_girl_pose(k) == ML_POSE_STAND) == (k < ML_SHOT_M4B));
        }
        CHECK(ML_GIRL_SEATED_QUADS <= ML_GIRL_MAX_QUADS && ML_GIRL_QUADS <= ML_GIRL_MAX_QUADS);
        CHECK(ml_camera_at(ML_SHOT_M4B, 0.0f, &c) == 1 && c.hfov_deg == 34.0f);
        ml_girl_build_seated(&c, pos, sq);
        for (i = 0; i < ML_GIRL_SEATED_QUADS; i++) {
            const float ex = sq[i].p[1][0] - sq[i].p[0][0], ey = sq[i].p[1][1] - sq[i].p[0][1], ez = sq[i].p[1][2] - sq[i].p[0][2];
            const float fx = sq[i].p[2][0] - sq[i].p[0][0], fy = sq[i].p[2][1] - sq[i].p[0][1], fz = sq[i].p[2][2] - sq[i].p[0][2];

            CHECK(sh4der_len3_(ey * fz - ez * fy, ez * fx - ex * fz, ex * fy - ey * fx) > 1.0e-6f);
            for (k = 0; k < 4; k++) {
                s_top = fmaxf(s_top, sq[i].p[k][1]);
                s_bottom = fminf(s_bottom, sq[i].p[k][1]);
                CHECK(fabsf(ml_girl_seated_pts[i][k][0]) <= 0.45f);
            }
        }
        CHECK(s_top < ML_GIRL_SEATED_HEIGHT && fabsf(s_bottom) < 0.001f && ML_GIRL_SEATED_HEIGHT < ML_GIRL_HEIGHT);
        printf("  seated girl (M4b, %.0f deg): %.2f m high = %.0f pixels at %.1f m\n", (double)c.hfov_deg, (double)ML_GIRL_SEATED_HEIGHT,
               (double)(ML_GIRL_SEATED_HEIGHT * c.focal_px / sqrtf(sh4der_len3_(pos[0] - c.eye[0], 0.0f, pos[2] - c.eye[2]))),
               (double)sqrtf(sh4der_len3_(pos[0] - c.eye[0], 0.0f, pos[2] - c.eye[2])));
    }
    /* Footprints: alternate sides, a stride apart, the whole walk in ML_STEPS_TOTAL prints. */
    {
        float x0, z0, x1, z1;

        ml_foot(0, &x0, &z0);
        ml_foot(1, &x1, &z1);
        CHECK(x0 < 0.0f && x1 > 0.0f && fabsf((z0 - z1) - ML_STRIDE) <= 2.0f * ML_FOOT_JITTER_Z + 1.0e-4f);
        CHECK(fabsf(z0 - ML_GIRL_WALK_Z0) < 1.0e-4f);            /* the first print is where the walk starts */
        /* no two prints alike: each has its own place, depth, size, length and turn, inside the stated ranges, and the
         * prints stay apart along the walk */
        {
            float d_min = 1.0f, d_max = 0.0f, r_min = 1.0f, r_max = 0.0f;
            unsigned n;

            for (n = 0u; n < ML_STEPS_TOTAL; n++) {
                float x, z, depth, radius, half_gap, turn;

                ml_foot(n, &x, &z);
                ml_foot_shape(n, &depth, &radius, &half_gap, &turn);
                CHECK(fabsf(fabsf(x) - ML_FOOT_SIDE) <= ML_FOOT_JITTER_X && ((n & 1u) ? x > 0.0f : x < 0.0f));
                CHECK(fabsf(z - (ML_GIRL_WALK_Z0 - (float)n * ML_STRIDE)) <= ML_FOOT_JITTER_Z + 1.0e-5f);
                CHECK(depth >= ML_FOOT_DEPTH_MIN * ML_PRINT_DEPTH && depth <= ML_PRINT_DEPTH);
                CHECK(radius >= ML_FOOT_SIZE_MIN * ML_PRINT_RADIUS && radius <= ML_PRINT_RADIUS);
                CHECK(half_gap >= ML_FOOT_GAP_MIN * ML_PRINT_HALF_GAP && half_gap <= ML_PRINT_HALF_GAP && fabsf(turn) <= ML_FOOT_TURN);
                d_min = depth < d_min ? depth : d_min;
                d_max = depth > d_max ? depth : d_max;
                r_min = radius < r_min ? radius : r_min;
                r_max = radius > r_max ? radius : r_max;
            }
            CHECK(d_max - d_min > 0.25f * ML_PRINT_DEPTH && r_max - r_min > 0.05f * ML_PRINT_RADIUS);   /* a real spread */
            /* one print's pit ends at least 0.1 m before the next begins, however both are moved: separate prints, no channel */
            CHECK(ML_STRIDE - 2.0f * (ML_PRINT_RADIUS + ML_PRINT_HALF_GAP) - 2.0f * ML_FOOT_JITTER_Z >= 0.1f);
            CHECK(ml_hash01(7u, 1u) == ml_hash01(7u, 1u) && ml_hash01(7u, 1u) != ml_hash01(8u, 1u) && ml_hash01(7u, 1u) != ml_hash01(7u, 2u));
            printf("  footprints: depth %.2f .. %.2f m, stamp radius %.2f .. %.2f m, rows %.2f m apart\n", (double)d_min, (double)d_max, (double)r_min, (double)r_max,
                   (double)(2.0f * ML_FOOT_SIDE));
        }
        ml_foot(ML_STEPS_TOTAL - 1u, &x1, &z1);
        CHECK(z1 >= ML_GIRL_STAND_Z - 1.0e-4f);                  /* the last print is not past where she stops */
        ml_foot(ML_STEPS_TOTAL, &x1, &z1);
        CHECK(z1 < ML_GIRL_STAND_Z);                             /* the next would be */
    }
    /* The lying pose: two quads with area, on the snow, low. The spirits: only in M9's last seconds, five quads with area,
     * rising ML_LIFT_RISE in ML_LIFT_TIME_S, the grandmother taller than the girl's spirit. */
    {
        ml_quad_t lq[ML_GIRL_LYING_QUADS] = { 0 }, sq[ML_SPIRIT_QUADS] = { 0 };
        float l_top = 0.0f;

        ml_girl_build_lying(&c, pos, lq);
        for (i = 0; i < ML_GIRL_LYING_QUADS; i++) {
            for (k = 0; k < 4; k++) {
                l_top = fmaxf(l_top, lq[i].p[k][1]);
                CHECK(lq[i].p[k][1] >= -0.001f);
            }
            CHECK(fabsf(lq[i].p[2][0] - lq[i].p[0][0]) + fabsf(lq[i].p[2][2] - lq[i].p[0][2]) > 0.3f && lq[i].p[1][1] > lq[i].p[0][1]);
        }
        CHECK(l_top <= ML_GIRL_LYING_HEIGHT && ML_GIRL_LYING_HEIGHT < 0.3f);
        for (k = 0; k < ML_SHOTS; k++) {
            CHECK(ml_spirit_level(k, 1.0f) == 0.0f);
            CHECK((ml_spirit_level(k, ML_LIFT_START_S + 2.0f) == 1.0f) == (k == ML_SHOT_M9));
        }
        CHECK(ml_spirit_level(ML_SHOT_M9, ML_LIFT_START_S + 0.5f) > 0.0f && ml_spirit_level(ML_SHOT_M9, ML_LIFT_START_S + 0.5f) < 1.0f);
        CHECK(ML_LIFT_START_S + ML_LIFT_TIME_S <= ml_shots[ML_SHOT_M9].end_s - ml_shots[ML_SHOT_M9].start_s);
        CHECK(fabsf(ml_spirit_height(ML_LIFT_START_S) - ML_LIFT_HOVER) < 1.0e-4f && ML_GIRL_LYING_QUADS == 2);
        CHECK(fabsf(ml_spirit_height(ML_LIFT_START_S + ML_LIFT_TIME_S) - (ML_LIFT_HOVER + ML_LIFT_RISE)) < 1.0e-4f);
        ml_spirit_build(&c, pos, 1.0f, sq);
        {
            /* in the M9 frame at the end of the rise: the grandmother about 46 % across, the girl's spirit about 53 %, the
             * grandmother's head about 8 % under the frame's top */
            ml_camera_t c9 = { 0 };
            ml_quad_t eq[ML_SPIRIT_QUADS] = { 0 };
            ml_screen_t a = { 0.0f, 0.0f, 0.0f }, t = { 0.0f, 0.0f, 0.0f };
            float mid[3];

            CHECK(ml_camera_at(ML_SHOT_M9, 1.0f, &c9) == 1);
            ml_spirit_build(&c9, pos, ml_spirit_height(ML_LIFT_START_S + ML_LIFT_TIME_S), eq);
            for (k = 0; k < 3; k++) {
                mid[k] = 0.5f * (eq[1].p[1][k] + eq[1].p[3][k]);   /* the middle of the grandmother's head's top edge */
            }
            CHECK(fabsf(mid[1] - 3.2f) < 0.01f);                 /* the grandmother's head top ends 3.2 m over the ground */
            CHECK(ml_project(&c9, mid, &t) == 1 && BAND_PCT(t.y) > 6.0f && BAND_PCT(t.y) < 10.0f);   /* about 8 % under the band's top */
            CHECK(fabsf(t.x - 0.46f * 640.0f) < 10.0f);
            for (k = 0; k < 3; k++) {
                mid[k] = 0.5f * (eq[3].p[1][k] + eq[3].p[3][k]);   /* the girl's spirit's head */
            }
            CHECK(ml_project(&c9, mid, &a) == 1 && fabsf(a.x - 0.53f * 640.0f) < 10.0f);
        }
        for (i = 0; i < ML_SPIRIT_QUADS; i++) {
            CHECK(sq[i].p[1][1] > sq[i].p[0][1] && sq[i].p[3][1] > sq[i].p[2][1] && sq[i].p[0][1] >= 1.0f);   /* upper edge over lower, all lifted */
        }
        CHECK(sq[1].p[1][1] > sq[3].p[1][1] + 0.4f);            /* the grandmother stands taller */
        printf("  lying girl %.2f m high; spirits appear %.1f s into M9, rise %.1f m in %.1f s, grandmother %.2f m, girl %.2f m\n",
               (double)ML_GIRL_LYING_HEIGHT, (double)ML_LIFT_START_S, (double)ML_LIFT_RISE, (double)ML_LIFT_TIME_S,
               (double)(sq[1].p[1][1] - 1.0f), (double)(sq[3].p[1][1] - 1.0f));
    }
    CHECK(ml_steps_due(ML_SHOT_M1, 0.5f) == 0u && ml_steps_due(ML_SHOT_M2, 0.0f) == 1u && ml_steps_due(ML_SHOT_M2, 1.0f) == ML_STEPS_TOTAL &&
          ml_steps_due(ML_SHOT_M2B, 0.0f) == ML_STEPS_TOTAL && ml_steps_due(ML_SHOT_M6, 0.0f) == ML_STEPS_TOTAL);
    CHECK(ML_STEPS_TOTAL == (unsigned)(ML_WALK_DIST / ML_STRIDE) + 1u);
    /* A print sinks in: untouched when her foot arrives, deeper every moment after, complete before the next foot lands. */
    {
        const float per_m = 1.0f / ML_WALK_DIST;          /* M2 progress per metre walked */
        const float m2_s = ml_shots[ML_SHOT_M2].end_s - ml_shots[ML_SHOT_M2].start_s;
        const float sink_m = ml_step_sound_s[ml_step_sound(3u)] * ML_WALK_DIST / m2_s;   /* metres walked while print 3's sound plays */
        float last = 0.0f;
        unsigned k;

        for (k = 0u; k < 4u; k++) {
            CHECK(ml_step_sound_s[k] > 0.0f && ml_step_sound_s[k] * ML_WALK_DIST / m2_s < ML_STRIDE);
        }
        CHECK(ml_step_press(ML_SHOT_M1, 0.9f, 0u) == 0.0f && ml_step_press(ML_SHOT_M2B, 0.0f, ML_STEPS_TOTAL - 1u) == 1.0f);
        CHECK(ml_step_press(ML_SHOT_M2, 3.0f * ML_STRIDE * per_m, 3u) < 0.01f);
        for (k = 1u; k <= 8u; k++) {
            const float p = ml_step_press(ML_SHOT_M2, (3.0f * ML_STRIDE + sink_m * (float)k / 8.0f) * per_m, 3u);

            CHECK(p > last && p <= 1.0f);
            last = p;
        }
        CHECK(last > 0.99f);
    }
}

/* Sky, ambient, snow light, fog. */
static void light(void) {
    float sky0[3], sky1[3], amb[3], rgb_near[3], rgb_far[3], rgb_off[3], rgb_dent[3];
    int k;

    ml_sky(0, 0.5f, sky0);
    CHECK(fabsf(sky0[0] * 255.0f - 20.0f) < 0.01f && fabsf(sky0[1] * 255.0f - 28.0f) < 0.01f && fabsf(sky0[2] * 255.0f - 56.0f) < 0.01f);
    ml_sky(ML_SHOT_M10, 0.0f, sky0);                             /* M10 starts night and ends cool dawn */
    CHECK(fabsf(sky0[2] * 255.0f - 56.0f) < 0.01f);
    ml_sky(ML_SHOT_M10, 1.0f, sky1);
    CHECK(fabsf(sky1[0] * 255.0f - 96.0f) < 0.01f && fabsf(sky1[1] * 255.0f - 112.0f) < 0.01f && fabsf(sky1[2] * 255.0f - 136.0f) < 0.01f);
    CHECK(sky1[2] > sky1[0]);                                    /* cool: blue over red, no warmth */
    ml_ambient(2, 0.5f, amb);
    for (k = 0; k < 3; k++) {
        CHECK(amb[k] > 0.3f && amb[k] < 0.6f);
    }
    ml_snow_colour(amb, 1.0f, 0.0f, 1.2f, 0.0f, rgb_near);       /* on the flame */
    ml_snow_colour(amb, 1.0f, 36.0f, 1.2f, 0.0f, rgb_far);       /* 6 m away: ambient only */
    ml_snow_colour(amb, 0.0f, 0.0f, 1.2f, 0.0f, rgb_off);        /* flame not lit */
    ml_snow_colour(amb, 0.0f, 0.0f, 1.2f, 1.0f, rgb_dent);       /* a full dent */
    CHECK(rgb_near[0] > rgb_far[0] + 0.2f);                      /* warm term lights it */
    CHECK(rgb_near[0] > rgb_near[2] + 0.1f);                     /* and it is warm: red over blue */
    CHECK(rgb_far[2] > rgb_far[0]);                              /* the unlit snow is cool */
    for (k = 0; k < 3; k++) {
        CHECK(fabsf(rgb_far[k] - rgb_off[k]) < 1.0e-5f);        /* beyond the reach, or unlit: the same */
        CHECK(rgb_dent[k] < rgb_off[k]);                         /* a dent is darker */
    }
    CHECK(ml_fog_keep(0, 5.0f) == 1.0f && ml_fog_keep(0, ML_FOG_END_Z + 1.0f) == 0.0f);
    CHECK(ml_fog_keep(0, 27.5f) > 0.4f && ml_fog_keep(0, 27.5f) < 0.6f);
}

/* The cut overlay: nothing in the middle of a shot; black at a shot's start and end exactly when its in_s / out_s say so;
 * a "dissolve" is a fade out and a fade in that meet at black; the film fades up from black and down to black; a struck
 * match opens its shot on two frames of gold-white. */
static void overlay(void) {
    float a;
    uint8_t rgb[3];
    int k;

    for (k = 0; k < ML_SHOTS; k++) {
        const ml_shot_t *s = &ml_shots[k];
        const float len = s->end_s - s->start_s;

        CHECK(s->in_s >= 0.0f && s->out_s >= 0.0f && s->in_s + s->out_s < len);
        ml_overlay(k, 0.5f * len, &a, rgb);
        CHECK(a == 0.0f);                                        /* nothing in the middle of a shot */
        ml_overlay(k, len - 0.001f, &a, rgb);
        CHECK(s->out_s > 0.0f ? (a > 0.95f && rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0) : a == 0.0f);
        ml_overlay(k, 0.001f, &a, rgb);
        if (s->flash) {
            CHECK(a > 0.8f && rgb[0] == 255 && rgb[1] > 200 && rgb[2] > 150 && rgb[2] < rgb[0]);   /* first frame: the flash */
            ml_overlay(k, 0.05f, &a, rgb);                       /* second frame: half */
            CHECK(a > 0.3f && a < 0.6f);
            ml_overlay(k, 0.5f, &a, rgb);                        /* gone */
            CHECK(a == 0.0f);
        } else {
            CHECK(s->in_s > 0.0f ? (a > 0.95f && rgb[0] == 0) : a == 0.0f);
        }
        if (k + 1 < ML_SHOTS && !ml_shots[k + 1].flash) {
            CHECK(s->out_s > 0.0f && ml_shots[k + 1].in_s > 0.0f);   /* a cut that is not a strike is a dissolve */
        }
    }
    CHECK(ml_shots[ML_SHOT_M1].in_s == 1.0f && ml_shots[ML_SHOT_M10].out_s == 1.0f);   /* the film's own fade up and down */
    CHECK(ml_shots[ML_SHOT_M9].out_s + ml_shots[ML_SHOT_M10].in_s == 2.0f);            /* the long dissolve out of M9 */
}

/* The sprite atlas: bright in the middle of the glow cell, black at its corners, every value in 0..1. */
static void atlas(void) {
    unsigned x, y;
    float lo = 1.0f, hi = 0.0f;

    for (y = 0; y < ML_ATLAS_SIDE; y++) {
        for (x = 0; x < ML_ATLAS_SIDE; x++) {
            const float v = ml_atlas_value(x, y);

            CHECK(v >= 0.0f && v <= 1.0f);
            if (v < lo) {
                lo = v;
            }
            if (v > hi) {
                hi = v;
            }
        }
    }
    CHECK(ml_atlas_value(15, 15) > 0.8f);                        /* glow centre */
    CHECK(ml_atlas_value(0, 0) == 0.0f && ml_atlas_value(31, 31) < 0.2f);   /* corners are dark: additive adds nothing */
    CHECK(ml_atlas_value(47, 15) > 0.5f);                        /* star centre */
    CHECK(ml_atlas_value(47, 3) < ml_atlas_value(47, 15));       /* the star's arm is thinner than its centre */
    CHECK(hi > 0.8f && lo == 0.0f);
}

/* Samples taken along one sight line. */
#define SIGHT_STEPS 400
/* A sight line must pass at least this far from every house, metres, and over its first 95 % stay higher over the snow
 * than this share of the lens height (a line that ends on the ground is at 5 % of the lens height there, on level snow). */
#define SIGHT_HOUSE_MIN 1.0f
#define SIGHT_SNOW_SHARE 0.04f

/* Walks the straight line from `eye` to `subject` and reports how near it comes to a house and to the snow.
 * *house: the smallest horizontal distance from the line to any house box, taken only where the line is below that
 * house's gable peak (0 = the line goes through a house). *snow: the smallest height of the line over the snow surface,
 * taken over the first 95 % of the line (the subject itself may stand on the snow). */
static void sight_measure(const float eye[3], const float subject[3], float *house, float *snow) {
    int i;
    unsigned h;

    *house = 1.0e9f;
    *snow = 1.0e9f;
    for (i = 0; i <= SIGHT_STEPS; i++) {
        const float t = (float)i / (float)SIGHT_STEPS;
        const float x = sh4der_lerp_(eye[0], subject[0], t);
        const float y = sh4der_lerp_(eye[1], subject[1], t);
        const float z = sh4der_lerp_(eye[2], subject[2], t);

        for (h = 0; h < ML_HOUSES; h++) {
            const ml_house_t *s = &ml_houses[h];
            const float xa = s->face_x < 0.0f ? s->face_x - s->depth : s->face_x;   /* the box's two x faces */
            const float xb = s->face_x < 0.0f ? s->face_x : s->face_x + s->depth;
            const float dx = x < xa ? xa - x : (x > xb ? x - xb : 0.0f);
            const float dz = z < s->z0 ? s->z0 - z : (z > s->z0 + s->width ? z - (s->z0 + s->width) : 0.0f);
            const float d = sqrtf(sh4der_len2_(dx, dz));

            if (y < s->height + s->gable && d < *house) {
                *house = d;
            }
        }
        if (t <= 0.95f && y - ml_snow_height(x, z) < *snow) {
            *snow = y - ml_snow_height(x, z);
        }
    }
}

/* Line of sight: in every shot, at the start, middle and end of its camera move, the straight line from the lens to the
 * subject (the girl's middle; in M6 the tree's middle too; in M2 and M2b the first and the last footprint too) stays 1 m or more
 * from every house and clear of the snow. Prints one row a shot: camera, subject, nearest house, least height over snow. */
static void sight_lines(void) {
    int k, i;

    printf("  line of sight, per shot (worst of start / middle / end of the camera move):\n");
    for (k = 0; k < ML_SHOTS; k++) {
        float worst_house = 1.0e9f, worst_snow = 1.0e9f;
        ml_camera_t c = { 0 };
        float girl[3];

        for (i = 0; i <= 2; i++) {
            const float p = 0.5f * (float)i;
            float subject[4][3];
            int n = 1, j;

            CHECK(ml_camera_at(k, p, &c) == 1);
            CHECK(fabsf(c.eye[0]) <= ML_LANE_HALF - 2.0f);       /* the camera stands inside the lane */
            ml_girl_centre(k, p, subject[0]);
            if (ml_shots[k].has_tree) {
                subject[n][0] = ML_TREE_X; subject[n][1] = 1.5f; subject[n][2] = ML_TREE_Z;
                n++;
            }
            if (k == ML_SHOT_M2 || k == ML_SHOT_M2B) {           /* M2 and M2b: the trail of prints, both ends */
                ml_foot(0, &subject[n][0], &subject[n][2]);
                subject[n][1] = 0.0f;
                n++;
                ml_foot(ML_STEPS_TOTAL - 1u, &subject[n][0], &subject[n][2]);
                subject[n][1] = 0.0f;
                n++;
            }
            for (j = 0; j < n; j++) {
                float house, snow;

                sight_measure(c.eye, subject[j], &house, &snow);
                CHECK(house >= SIGHT_HOUSE_MIN);
                CHECK(snow >= SIGHT_SNOW_SHARE * c.eye[1]);
                if (house < worst_house) {
                    worst_house = house;
                }
                if (snow < worst_snow) {
                    worst_snow = snow;
                }
            }
        }
        CHECK(ml_camera_at(k, 0.0f, &c) == 1);
        ml_girl_centre(k, 0.0f, girl);
        printf("    %-3s camera (%5.2f, %4.2f, %5.2f) %4.1f m from her  girl (%3.1f, %4.2f, %4.1f)  nearest house %4.2f m  least height over snow %4.2f m\n",
               ml_shots[k].name, (double)c.eye[0], (double)c.eye[1], (double)c.eye[2],
               (double)sqrtf(sh4der_len2_(girl[0] - c.eye[0], girl[2] - c.eye[2])), (double)girl[0], (double)girl[1], (double)girl[2],
               (double)worst_house, (double)worst_snow);
    }
    for (k = 0; k < ML_HOUSES; k++) {
        CHECK(fabsf(ml_houses[k].face_x) >= ML_LANE_HALF);       /* an open lane: every front pushed back */
    }
    /* M4b: her corner is fog only. Only this shot has the closed fog; she is in front of it; every house front point that
     * lands inside the frame lies deeper than the fog's end, so it is drawn in the fog's colour, which is the background's
     * colour too: no front and no edge can show. Prints where the front steps land (they are in the fog). */
    {
        ml_camera_t c = { 0 };
        float g[3], v[3], start, end, sky[3], nearest = 1.0e9f;
        unsigned h;
        int i;

        for (k = 0; k < ML_SHOTS; k++) {
            CHECK(ml_fog_closed(k) == (k == ML_SHOT_M4B));
        }
        ml_fog_range(ML_SHOT_M4B, &start, &end);
        CHECK(ml_camera_at(ML_SHOT_M4B, 0.0f, &c) == 1);
        ml_girl_pos(ML_SHOT_M4B, 0.0f, g);
        ml_to_view(&c, g, v);
        CHECK(v[2] + 0.5f < start);                              /* she, and half a metre behind her, are clear of the fog */
        ml_sky(ML_SHOT_M4B, 0.5f, sky);
        CHECK(sky[0] == ML_FOG_R && sky[1] == ML_FOG_G && sky[2] == ML_FOG_B);
        for (h = 0; h < ML_HOUSES; h++) {
            const ml_house_t *s = &ml_houses[h];

            for (i = 0; i <= 40; i++) {                         /* along the front and the near side wall, at the ground and the eaves */
                const float t = (float)i / 40.0f;
                const float away = s->face_x < 0.0f ? -s->depth : s->depth;
                const float pts[4][3] = {
                    { s->face_x, 0.0f, s->z0 + t * s->width }, { s->face_x, s->height + s->gable, s->z0 + t * s->width },
                    { s->face_x + t * away, 0.0f, s->z0 }, { s->face_x + t * away, s->height, s->z0 },
                };
                int j;

                for (j = 0; j < 4; j++) {
                    ml_screen_t sc = { 0.0f, 0.0f, 0.0f };

                    ml_to_view(&c, pts[j], v);
                    if (ml_view_to_screen(&c, v, &sc) && sc.x >= 0.0f && sc.x < (float)ML_SCREEN_W) {
                        CHECK(v[2] >= end);
                        nearest = fminf(nearest, v[2]);
                        if (j == 0 && i == 0) {
                            printf("    M4b: house front edge at z %.1f lands at %.0f %% of the frame width, %.1f m deep: in the fog\n",
                                   (double)s->z0, (double)(100.0f * sc.x / (float)ML_SCREEN_W), (double)v[2]);
                        }
                    }
                }
            }
        }
        printf("    M4b closed fog %.1f to %.1f m; nearest house point in frame %.1f m deep (all in full fog)\n", (double)start, (double)end, (double)nearest);
    }
    /* The step between the third and the fourth house front of the left side stands behind her, left of the frame's
     * middle (35 to 40 % of the width): it is where the corner is, inside the fog. */
    {
        const float corner[3] = { ml_houses[2].face_x, 1.0f, ml_houses[2].z0 + ml_houses[2].width };
        ml_camera_t c = { 0 };
        ml_screen_t s = { 0.0f, 0.0f, 0.0f };

        CHECK(ml_camera_at(ML_SHOT_M4B, 0.0f, &c) == 1 && ml_project(&c, corner, &s) == 1);
        CHECK(s.x > 0.30f * (float)ML_SCREEN_W && s.x < 0.45f * (float)ML_SCREEN_W);
        printf("    M4b corner (house front step at z %.1f) lands at %.0f %% of the frame width\n", (double)corner[2], (double)(100.0f * s.x / (float)ML_SCREEN_W));
    }
}

/* The snow is shaped: level on the girl's path, never below the path, banked against the walls, with drifts and piles;
 * the shade is 1 on the level and darker on a slope that faces the cameras. Prints the height numbers. */
static void snow_shape(void) {
    float x, z, highest = 0.0f, lane_sum = 0.0f, bank_lo = 1.0e9f, bank_hi = 0.0f, sx, sz;
    unsigned lane_n = 0, i;

    for (z = ML_GROUND_Z0; z <= ML_GROUND_Z0 + 40.0f; z += 0.25f) {
        for (x = ML_GROUND_X0; x <= -ML_GROUND_X0; x += 0.25f) {
            const float h = ml_snow_height(x, z);

            CHECK(h >= 0.0f && h < 2.5f);
            if (fabsf(x) <= ML_PATH_FLAT) {
                CHECK(h == 0.0f);                                /* the path is level */
            }
            if (fabsf(x) < ML_LANE_HALF) {
                lane_sum += h;
                lane_n++;
            }
            if (h > highest) {
                highest = h;
            }
        }
        bank_lo = fminf(bank_lo, fminf(ml_snow_height(-ML_LANE_HALF, z), ml_snow_height(ML_LANE_HALF, z)));
        bank_hi = fmaxf(bank_hi, fmaxf(ml_snow_height(-ML_LANE_HALF, z), ml_snow_height(ML_LANE_HALF, z)));
    }
    CHECK(highest > 0.9f);                                       /* real drifts, not a plane */
    CHECK(bank_lo > 0.3f && bank_hi < 2.2f);
    for (i = 0; i < ML_MOUNDS; i++) {
        CHECK(fabsf(ml_mounds[i][0]) - ml_mounds[i][2] >= ML_PATH_FLAT);   /* no pile reaches the path */
        CHECK(ml_snow_height(ml_mounds[i][0], ml_mounds[i][1]) >= ml_mounds[i][3]);
    }
    CHECK(ml_snow_height(0.0f, ML_GIRL_STAND_Z) == 0.0f && ml_snow_height(0.0f, ML_GIRL_FAR_Z) == 0.0f);
    ml_snow_slope(0.0f, 15.0f, &sx, &sz);
    CHECK(sx == 0.0f && sz == 0.0f && ml_snow_shade(sx, sz) == 1.0f);
    CHECK(ml_snow_shade(0.0f, 1.0f) < 0.7f);                     /* rises away from the cameras: faces them, in shade */
    CHECK(ml_snow_shade(0.0f, -1.0f) > 1.1f);                    /* faces the moon */
    CHECK(ml_snow_shade(0.0f, 50.0f) >= ML_SHADE_MIN && ml_snow_shade(-50.0f, -50.0f) <= ML_SHADE_MAX);
    printf("  snow: highest %.2f m, mean over the lane %.2f m, wall banks %.2f to %.2f m, drifts 0 to %.2f m, %u piles %.2f to %.2f m\n",
           (double)highest, (double)(lane_sum / (float)lane_n), (double)bank_lo, (double)bank_hi, (double)(2.0f * ML_ROLL_HEIGHT), ML_MOUNDS,
           (double)ml_mounds[0][3], (double)ml_mounds[4][3]);
}

/* The trail patch replaces exactly the ground quads it covers, and every footprint pit (with its rim) lies inside the
 * patch's inner vertices. Prints how large a pit is on screen at the M2 camera and how dark it is. */
static void trail_and_prints(void) {
    const float reach = ML_PRINT_RADIUS * 1.6f;                  /* the rim ends at 1.6 stamp radii (sh4der_deform's rim reach) */
    const float x1 = ML_TRAIL_X0 + (float)ML_TRAIL_COLS * ML_TRAIL_CELL;
    const float z1 = ML_TRAIL_Z0 + (float)ML_TRAIL_ROWS * ML_TRAIL_CELL;
    float amb[3], flat[3], pit[3];
    ml_camera_t c = { 0 };
    unsigned n;
    int k;

    CHECK(ML_TRAIL_X0 == ML_GROUND_X0 + (float)ML_TRAIL_GROUND_COL * ML_GROUND_DX && x1 == ML_TRAIL_X0 + ML_GROUND_DX);
    CHECK(ML_TRAIL_Z0 == ML_GROUND_Z0 + (float)ML_TRAIL_GROUND_ROW0 * ML_GROUND_DZ && z1 == ML_TRAIL_Z0 + (float)ML_TRAIL_GROUND_ROWS * ML_GROUND_DZ);
    CHECK(x1 <= ML_PATH_FLAT && -ML_TRAIL_X0 <= ML_PATH_FLAT);   /* the patch lies on the level path */
    CHECK(ml_ground_quad_in_trail(12, 16) && ml_ground_quad_in_trail(12, 26) && !ml_ground_quad_in_trail(12, 27) && !ml_ground_quad_in_trail(11, 20));
    CHECK(ML_PRINT_DEPTH < ML_SNOW_DEPTH);
    for (n = 0; n < ML_STEPS_TOTAL; n++) {
        float x, z;

        ml_foot(n, &x, &z);
        CHECK(fabsf(x) + reach <= x1 - ML_TRAIL_CELL);
        CHECK(z - ML_PRINT_HALF_GAP - reach >= ML_TRAIL_Z0 + ML_TRAIL_CELL && z + ML_PRINT_HALF_GAP + reach <= z1 - ML_TRAIL_CELL);
    }
    ml_ambient(ML_SHOT_M2, 0.5f, amb);
    ml_snow_colour(amb, 0.0f, 0.0f, 1.0f, 0.0f, flat);
    ml_snow_colour(amb, 0.0f, 0.0f, 1.0f, ML_PRINT_DEPTH / ML_SNOW_DEPTH, pit);
    CHECK(pit[1] < 0.55f * flat[1]);                             /* the pit's floor is under 55 % as bright as level snow */
    printf("  footprint pit: %.2f m wide, %.2f m long, %.2f m deep, %u prints %.2f m apart\n", (double)(2.0f * ML_PRINT_RADIUS),
           (double)(2.0f * (ML_PRINT_RADIUS + ML_PRINT_HALF_GAP)), (double)ML_PRINT_DEPTH, ML_STEPS_TOTAL, (double)ML_STRIDE);
    for (k = ML_SHOT_M2; k <= ML_SHOT_M2B; k++) {
        CHECK(ml_camera_at(k, 0.0f, &c) == 1);
        for (n = 0; n < ML_STEPS_TOTAL; n += ML_STEPS_TOTAL - 1u) {
            float x, z, d;

            ml_foot(n, &x, &z);
            d = sqrtf(sh4der_len3_(x - c.eye[0], c.eye[1], z - c.eye[2]));
            printf("    at the %s camera, print %2u is %.1f m away: %.0f pixels wide, its far wall %.0f pixels high\n", ml_shots[k].name, n + 1u,
                   (double)d, (double)(2.0f * ML_PRINT_RADIUS * c.focal_px / d), (double)(ML_PRINT_DEPTH * c.focal_px / d));
            CHECK(2.0f * ML_PRINT_RADIUS * c.focal_px / d >= 8.0f);     /* the farthest print is still 8 pixels wide */
        }
    }
    printf("    level snow at night (%.0f, %.0f, %.0f), pit floor (%.0f, %.0f, %.0f) = %.0f %%, pit wall facing the camera down to %.0f %%\n",
           (double)(flat[0] * 255.0f), (double)(flat[1] * 255.0f), (double)(flat[2] * 255.0f), (double)(pit[0] * 255.0f), (double)(pit[1] * 255.0f),
           (double)(pit[2] * 255.0f), (double)(100.0f * pit[1] / flat[1]), (double)(100.0f * ML_SHADE_MIN * pit[1] / flat[1]));
}

/* Every shot has a caption, and "<name> <caption>" fits the caption image. */
static void captions(void) {
    int k;

    for (k = 0; k < ML_SHOTS; k++) {
        CHECK(ml_shots[k].caption != NULL && strlen(ml_shots[k].caption) > 0u);
        CHECK(strlen(ml_shots[k].name) + 1u + strlen(ml_shots[k].caption) <= (size_t)ML_CAPTION_CHARS);
        printf("  caption: %s %s\n", ml_shots[k].name, ml_shots[k].caption);
    }
}

/* The sound cues: every sound has its own file name, every cue names a real shot, a real sound and a time inside its
 * shot; every match that is struck has its strike on match_on_s; a cue falls due exactly once as the clock passes it. */
static void sound_cues(void) {
    int struck[ML_SHOTS];
    unsigned i, j;
    int k;

    memset(struck, 0, sizeof(struck));
    for (i = 0; i < ML_SND_COUNT; i++) {
        CHECK(ml_sound_names[i] != NULL && strlen(ml_sound_names[i]) > 0u && strlen(ml_sound_names[i]) <= 8u);
        for (j = 0; j < i; j++) {
            CHECK(strcmp(ml_sound_names[i], ml_sound_names[j]) != 0);
        }
    }
    CHECK(ML_SND_STEP0 + ML_SND_STEPS - 1u == ML_SND_STEP3);
    for (i = 0; i < ML_CUES; i++) {
        const ml_cue_t *c = &ml_cues[i];
        const ml_shot_t *s;

        CHECK(c->shot < ML_SHOTS && c->sound < ML_SND_COUNT && c->vol > 0u);
        CHECK(c->sound != ML_SND_WIND && c->sound != ML_SND_CRACKLE);   /* the loops are not cues */
        s = &ml_shots[c->shot];
        CHECK(c->at_s >= 0.0f && c->at_s < s->end_s - s->start_s);
        if (c->sound == ML_SND_STRIKE || c->sound == ML_SND_FLARE) {
            CHECK(c->at_s == s->match_on_s);
            struck[c->shot]++;
        }
        CHECK(ml_cue_due(c, c->shot, -1.0f, c->at_s) == 1);              /* due as the clock arrives */
        CHECK(ml_cue_due(c, c->shot, c->at_s, c->at_s + 0.03f) == 0);    /* not again on the next frame */
        CHECK(ml_cue_due(c, c->shot, -1.0f, c->at_s - 0.001f) == 0 || c->at_s == 0.0f);   /* not before its time */
        CHECK(ml_cue_due(c, (c->shot + 1) % ML_SHOTS, -1.0f, 100.0f) == 0);   /* not in another shot */
    }
    for (k = 0; k < ML_SHOTS; k++) {
        /* a match is struck in every shot that opens on a flash (M4 has none: it opens on the last of M3's flame) */
        CHECK(struck[k] == (ml_shots[k].flash ? 1 : 0));
    }
    CHECK(ml_cue_due(NULL, 0, -1.0f, 1.0f) == 0);
    /* The held sounds: a real sound, inside its shot, the fade of 0.325 s inside the film. */
    CHECK(ML_HOLD_FADE_S == 0.325f && ML_HOLDS == 3u);
    for (i = 0; i < (int)ML_HOLDS; i++) {
        const ml_hold_t *h = &ml_holds[i];

        CHECK(h->shot < ML_SHOTS && h->sound < ML_SND_COUNT && h->vol > 0u && h->start_s >= 0.0f && h->end_s > h->start_s);
        CHECK(h->end_s <= ml_shots[h->shot].end_s - ml_shots[h->shot].start_s);
        CHECK(ml_shots[h->shot].start_s + h->end_s + ML_HOLD_FADE_S <= ML_FILM_SECONDS);
    }
    /* the lights' sound covers the candles' flight in M6 and the lift in M9; the sizzle's fade ends on the cut to M6's strike */
    CHECK(ml_holds[1].shot == ML_SHOT_M6 && ml_holds[1].start_s == 4.0f && ml_holds[1].end_s == ML_CANDLES_DONE_S);
    CHECK(ml_holds[2].shot == ML_SHOT_M9 && ml_holds[2].start_s == ML_LIFT_START_S && ml_holds[2].end_s == ML_LIFT_START_S + ML_LIFT_TIME_S);
    CHECK(ml_holds[0].shot == ML_SHOT_M5 && fabsf(ml_holds[0].end_s + ML_HOLD_FADE_S - ml_shots[ML_SHOT_M5].match_off_s) < 1.0e-4f);
    /* the wind is full through the film and fades out over the last shot's last 3 s */
    CHECK(ml_wind_volume(ML_SHOT_M5, 8.9f) == ML_VOL_WIND && ml_wind_volume(ML_SHOT_M10, 8.9f) == ML_VOL_WIND);
    CHECK(ml_wind_volume(ML_SHOT_M10, 10.5f) == ML_VOL_WIND / 2 && ml_wind_volume(ML_SHOT_M10, 12.0f) == 0);
    printf("  sound: %d sounds, %u timed cues (footfalls and the two loops are played by state)\n", (int)ML_SND_COUNT, ML_CUES);
}

/* Prints the counted triangle budget per shot (counted from the tables, upper bound, no culling, no measurement). */
static void budget(void) {
    int k;
    unsigned worst = 0;

    for (k = 0; k < ML_SHOTS; k++) {
        const unsigned n = ml_tris_counted(k);

        printf("  %-3s counted triangles a frame (upper bound): %u\n", ml_shots[k].name, n);
        CHECK(n > 2000u && n < 4200u);
        if (n > worst) {
            worst = n;
        }
    }
    printf("  worst shot: %u triangles (counted, not measured)\n", worst);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    timeline();
    match_state();
    cameras();
    projection();
    set_tables();
    girl();
    light();
    overlay();
    atlas();
    sight_lines();
    snow_shape();
    trail_and_prints();
    captions();
    sound_cues();
    budget();
    if (fails) {
        fprintf(stderr, "matchlight_scene_test: %d FAILED\n", fails);
        return 1;
    }
    puts("matchlight_scene: PASS timeline (12 shots, 120 s, loops), match state, cameras (girl >= 6 m and in frame), projection, set tables, girl and footprints, light, cut overlay, atlas, sight lines, snow shape, trail and prints, captions, sound cues, budget");
    return 0;
}
