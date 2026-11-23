/* SH4DER-PVR: match girl film scene test

   test/matchlight_scene_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of demo/matchlight/matchlight_scene.h (the plain C path, -DSH4DER_CORE_NO_SH4ZAM): the timeline covers
   0 to 60 s with no gap or overlap and loops; the match shots are the ones the film spec names; every camera looks at a
   finite target, keeps the girl 9 m or more from the lens and keeps her middle inside the 640 x 480 frame all through its
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

/* Progress samples taken along each shot when its camera is checked: start, middle and end of any camera move, and
 * points between, so the crane and the pulls are covered. */
#define PROGRESS_STEPS 20

/* The timeline: the ten shots tile 0 .. 60 s with no gap or overlap, every time maps to a shot in order, and it loops. */
static void timeline(void) {
    ml_where_t w, prev;
    int k, last_shot = 0;
    float t;

    CHECK(ML_SHOTS == 10);
    CHECK(ml_shots[0].start_s == 0.0f);
    CHECK(ml_shots[ML_SHOTS - 1].end_s == ML_FILM_SECONDS);
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
    /* Looping: 60 s is the start again, and so is any whole number of films, and so is a negative time. */
    ml_locate(60.0f, &w);
    CHECK(w.shot == 0 && fabsf(w.clock) < 0.001f);
    ml_locate(63.5f, &w);
    CHECK(w.shot == 0 && fabsf(w.clock - 3.5f) < 0.001f);
    ml_locate(-1.0f, &w);
    CHECK(w.shot == ML_SHOTS - 1 && fabsf(w.clock - 59.0f) < 0.001f);
    ml_locate(12.0f, &w);                                       /* a boundary belongs to the shot that starts there */
    CHECK(w.shot == 2 && w.progress < 0.001f);
    ml_locate(59.999f, &w);
    CHECK(w.shot == 9 && w.progress > 0.99f);
}

/* The match state: lit in M3, M5, M6, M8, M9; not in M1, M2, M7, M10; in M4 only the first 0.4 s (it goes out). */
static void match_state(void) {
    static const int lit_shots[ML_SHOTS] = { 0, 0, 1, 1, 1, 1, 0, 1, 1, 0 };
    int k;

    for (k = 0; k < ML_SHOTS; k++) {
        CHECK(ml_shot_lit(k) == lit_shots[k]);
        if (!lit_shots[k]) {
            CHECK(ml_match_level(k, 1.0f) == 0.0f);
        }
    }
    CHECK(ml_match_level(2, 0.1f) == 0.0f);                    /* M3: before the strike */
    CHECK(ml_match_level(2, 4.0f) == 1.0f);                    /* M3: held */
    CHECK(ml_match_level(3, 0.0f) == 1.0f);                    /* M4 opens on the last of the flame */
    CHECK(ml_match_level(3, 0.2f) > 0.0f && ml_match_level(3, 0.2f) < 1.0f);
    CHECK(ml_match_level(3, 0.45f) == 0.0f);                   /* and it is out within 0.4 s */
    CHECK(ml_shots[8].pool_radius >= 2.5f);                    /* M9 is the largest pool */
    CHECK(ml_shots[8].pool_radius > 1.5f * ml_shots[2].pool_radius);   /* and reads larger than M3 by at least 1.5 x */
    CHECK(ml_shots[5].has_tree == 1 && ml_shots[2].has_tree == 0);
}

/* Cameras: finite targets, the girl 9 m or more away, the girl's middle on screen, all along every move. */
static void cameras(void) {
    int k, i;
    float worst_dist = 1.0e9f;

    for (k = 0; k < ML_SHOTS; k++) {
        for (i = 0; i <= PROGRESS_STEPS; i++) {
            const float p = (float)i / (float)PROGRESS_STEPS;
            ml_camera_t c;
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
            CHECK(dist >= 9.0f);
            CHECK(ml_project(&c, centre, &s) == 1);
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
    ml_camera_t c;
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
    /* The M6 crane really moves up, with an ease: the middle is half way, the ends are the poses. */
    CHECK(ml_camera_at(5, 0.0f, &c) == 1 && fabsf(c.eye[1] - 1.6f) < 1.0e-4f);
    CHECK(ml_camera_at(5, 0.5f, &c) == 1 && fabsf(c.eye[1] - 2.8f) < 1.0e-4f);
    CHECK(ml_camera_at(5, 1.0f, &c) == 1 && fabsf(c.eye[1] - 4.0f) < 1.0e-4f);
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
            ml_quad_t q;
            int lit = -1;

            CHECK(ml_house_window(h, j, &q, &lit) == 1);
            lit_total += (unsigned)lit;
            lo = q.p[0][2]; hi = q.p[2][2];
            CHECK(lo >= h->z0 && hi <= h->z0 + h->width);        /* inside the wall along the street */
            CHECK(q.p[1][1] < h->height);                        /* under the eaves */
        }
        {
            ml_quad_t q;
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
    CHECK(ML_GIRL_QUADS == 4 && ML_TREE_CANDLES == 12u);
    for (i = 0; i < ML_TREE_CANDLES; i++) {
        CHECK(ml_tree_candles[i][1] > 0.0f && ml_tree_candles[i][1] < 3.0f);
    }
    printf("  houses %d left + %d right, windows %u (%u lit)\n", left, right, ml_count_windows(0), ml_count_windows(1));
}

/* The girl: about 1.1 m tall, every quad has area, and she is turned to the camera. */
static void girl(void) {
    ml_camera_t c;
    ml_quad_t q[ML_GIRL_QUADS];
    float pos[3] = { 0.0f, 0.0f, ML_GIRL_STAND_Z };
    float top = 0.0f, bottom = 1.0e9f;
    int i, k;

    CHECK(ml_camera_at(2, 0.0f, &c) == 1);
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
    CHECK(fabsf(top - ML_GIRL_HEIGHT) < 0.001f && fabsf(bottom) < 0.001f);
    /* Footprints: alternate sides, a stride apart, the whole walk in ML_STEPS_TOTAL prints. */
    {
        float x0, z0, x1, z1;

        ml_foot(0, &x0, &z0);
        ml_foot(1, &x1, &z1);
        CHECK(x0 < 0.0f && x1 > 0.0f && fabsf((z0 - z1) - ML_STRIDE) < 1.0e-4f);
        ml_foot(ML_STEPS_TOTAL - 1u, &x1, &z1);
        CHECK(z1 >= ML_GIRL_STAND_Z - 1.0e-4f);                  /* the last print is not past where she stops */
        ml_foot(ML_STEPS_TOTAL, &x1, &z1);
        CHECK(z1 < ML_GIRL_STAND_Z);                             /* the next would be */
    }
    CHECK(ml_steps_due(0, 0.5f) == 0u && ml_steps_due(1, 0.0f) == 1u && ml_steps_due(1, 1.0f) == ML_STEPS_TOTAL && ml_steps_due(5, 0.0f) == ML_STEPS_TOTAL);
    CHECK(ML_STEPS_TOTAL == (unsigned)(ML_WALK_DIST / ML_STRIDE) + 1u);
}

/* Sky, ambient, snow light, fog. */
static void light(void) {
    float sky0[3], sky1[3], amb[3], rgb_near[3], rgb_far[3], rgb_off[3], rgb_dent[3];
    int k;

    ml_sky(0, 0.5f, sky0);
    CHECK(fabsf(sky0[0] * 255.0f - 20.0f) < 0.01f && fabsf(sky0[1] * 255.0f - 28.0f) < 0.01f && fabsf(sky0[2] * 255.0f - 56.0f) < 0.01f);
    ml_sky(9, 0.0f, sky0);                                       /* M10 starts night and ends cool dawn */
    CHECK(fabsf(sky0[2] * 255.0f - 56.0f) < 0.01f);
    ml_sky(9, 1.0f, sky1);
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
    CHECK(ml_fog_keep(5.0f) == 1.0f && ml_fog_keep(ML_FOG_END_Z + 1.0f) == 0.0f);
    CHECK(ml_fog_keep(27.5f) > 0.4f && ml_fog_keep(27.5f) < 0.6f);
}

/* The cut overlay: a dip to black at every cut, 0.15 s wide in all, none mid-shot; the M9 flash is gold-white. */
static void overlay(void) {
    float a;
    uint8_t rgb[3];
    int k;

    for (k = 0; k < ML_SHOTS; k++) {
        const float len = ml_shots[k].end_s - ml_shots[k].start_s;

        ml_overlay(k, 0.5f * len, &a, rgb);
        CHECK(a == 0.0f);                                        /* nothing in the middle of a shot */
        ml_overlay(k, len - 0.001f, &a, rgb);
        CHECK(a > 0.95f && rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);   /* black at the end of a shot */
        if (k != ML_FLASH_SHOT) {
            ml_overlay(k, 0.001f, &a, rgb);
            CHECK(a > 0.95f && rgb[0] == 0);                    /* and at the start of the next */
        }
    }
    ml_overlay(ML_FLASH_SHOT, 0.01f, &a, rgb);                   /* M9: first frame is the flash */
    CHECK(a > 0.8f && rgb[0] == 255 && rgb[1] > 200 && rgb[2] > 150 && rgb[2] < rgb[0]);
    ml_overlay(ML_FLASH_SHOT, 0.05f, &a, rgb);                   /* second frame: half */
    CHECK(a > 0.3f && a < 0.6f);
    ml_overlay(ML_FLASH_SHOT, 0.5f, &a, rgb);                    /* gone */
    CHECK(a == 0.0f);
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
/* A sight line must pass at least this far from every house, metres, and this high over the snow, metres. */
#define SIGHT_HOUSE_MIN 1.0f
#define SIGHT_SNOW_MIN 0.05f

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
 * subject (the girl's middle; in M6 the tree's middle too; in M2 the first and the last footprint too) stays 1 m or more
 * from every house and clear of the snow. Prints one row a shot: camera, subject, nearest house, least height over snow. */
static void sight_lines(void) {
    int k, i;

    printf("  line of sight, per shot (worst of start / middle / end of the camera move):\n");
    for (k = 0; k < ML_SHOTS; k++) {
        float worst_house = 1.0e9f, worst_snow = 1.0e9f;
        ml_camera_t c;
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
            if (k == 1) {                                        /* M2: the trail of prints, both ends */
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
                CHECK(snow >= SIGHT_SNOW_MIN);
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
        printf("    %-3s camera (%5.1f, %3.1f, %4.1f)  girl (%3.1f, %4.2f, %4.1f)  nearest house %4.2f m  least height over snow %4.2f m\n",
               ml_shots[k].name, (double)c.eye[0], (double)c.eye[1], (double)c.eye[2], (double)girl[0], (double)girl[1], (double)girl[2],
               (double)worst_house, (double)worst_snow);
    }
    for (k = 0; k < ML_HOUSES; k++) {
        CHECK(fabsf(ml_houses[k].face_x) >= ML_LANE_HALF);       /* an open lane: every front pushed back */
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
    ml_camera_t c;
    unsigned n;

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
    ml_ambient(1, 0.5f, amb);
    ml_snow_colour(amb, 0.0f, 0.0f, 1.0f, 0.0f, flat);
    ml_snow_colour(amb, 0.0f, 0.0f, 1.0f, ML_PRINT_DEPTH / ML_SNOW_DEPTH, pit);
    CHECK(pit[1] < 0.55f * flat[1]);                             /* the pit's floor is under 55 % as bright as level snow */
    CHECK(ml_camera_at(1, 0.0f, &c) == 1);
    printf("  footprint pit: %.2f m wide, %.2f m long, %.2f m deep, %u prints %.2f m apart\n", (double)(2.0f * ML_PRINT_RADIUS),
           (double)(2.0f * (ML_PRINT_RADIUS + ML_PRINT_HALF_GAP)), (double)ML_PRINT_DEPTH, ML_STEPS_TOTAL, (double)ML_STRIDE);
    for (n = 0; n < ML_STEPS_TOTAL; n += ML_STEPS_TOTAL - 1u) {
        float x, z, d;

        ml_foot(n, &x, &z);
        d = sqrtf(sh4der_len3_(x - c.eye[0], c.eye[1], z - c.eye[2]));
        printf("    at the M2 camera, print %2u is %.1f m away: %.0f pixels wide, its far wall %.0f pixels high\n", n + 1u, (double)d,
               (double)(2.0f * ML_PRINT_RADIUS * c.focal_px / d), (double)(ML_PRINT_DEPTH * c.focal_px / d));
        CHECK(2.0f * ML_PRINT_RADIUS * c.focal_px / d >= 10.0f);
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

int main(void) {
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
    budget();
    if (fails) {
        fprintf(stderr, "matchlight_scene_test: %d FAILED\n", fails);
        return 1;
    }
    puts("matchlight_scene: PASS timeline (10 shots, 60 s, loops), match state, cameras (girl >= 9 m and in frame), projection, set tables, girl and footprints, light, cut overlay, atlas, sight lines, snow shape, trail and prints, captions, budget");
    return 0;
}
