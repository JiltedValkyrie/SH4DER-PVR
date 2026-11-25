/* SH4DER-PVR: ao4's scene C, the room (demo/psm/ao)

   demo/psm/ao/ao4_room.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The room of room_data.h (the living room's OBJ files, or the importer's placeholder room) as an ao4 scene: the floor
   first (it is the ground: the PVR map method's receiver), then the walls (each a grid, a strip a row), then the furniture, every
   triangle of it a strip of three. The AO bytes are the importer's bake (tools/room_bake.c), not ao4_bake's. The sun shades by N.L and casts
   nothing here: the room is an AO picture.

   One more way of drawing exists for this scene, AO4_LMAP: each flat surface as ONE quad with its lightmap (room_lm)
   in place of its grid of vertices; the furniture keeps its vertex bytes. ao4_room_quad gives a surface's corners.

   Needs ao4_scene.h first, and room_mesh.h / room_ao.h on the include path (build.py --room-only). */

#ifndef AO4_ROOM_H
#define AO4_ROOM_H

#include "room_data.h"

#if ROOM_V > AO4_V_MAX || ROOM_T > AO4_STRIP_MAX || ROOM_T * 3 > AO4_IDX_MAX || ROOM_CV > AO4_CV_MAX || ROOM_CT > AO4_CT_MAX
#error "the room is larger than ao4's arrays (ao4_scene.h)"
#endif
#define AO4_ROOM_SURF_MAX 8u
#define AO4_ROOM_CAM 0.7f             /* the eye this share of the way out along the model picture's line: the room fills the screen */
#if ROOM_SURFS > 8
#error "more than 8 flat surfaces: raise AO4_ROOM_SURF_MAX and the program's lightmap textures"
#endif

static inline void ao4_room_obj_(ao4_scene_t *s, unsigned k) {
    const room_obj_t *o = &room_obj[k];
    const unsigned base = s->vn;
    unsigned i;

    for (i = 0; i < o->vn; i++) {
        const unsigned v = o->v0 + i;

        ao4_vtx_(s, room_v[v][0], room_v[v][1], room_v[v][2], room_n[v][0], room_n[v][1], room_n[v][2], room_tint[v]);
        s->ao[base + i] = room_ao_v[v];
    }
    if (o->surf >= 0) {                     /* a flat surface's grid: a strip a row */
        const room_surf_t *f = &room_surf[o->surf];
        unsigned j;

        for (j = 0; j < f->nv; j++) {
            ao4_strip_begin_(s);
            for (i = 0; i <= f->nu; i++) { ao4_strip_idx_(s, base + (j + 1u) * (f->nu + 1u) + i); ao4_strip_idx_(s, base + j * (f->nu + 1u) + i); }
            ao4_strip_end_(s);
        }
        return;
    }
    for (i = 0; i < o->tn; i++) {
        const uint16_t *t = room_t[o->t0 + i];

        ao4_strip_begin_(s);
        ao4_strip_idx_(s, base + t[0] - o->v0); ao4_strip_idx_(s, base + t[1] - o->v0); ao4_strip_idx_(s, base + t[2] - o->v0);
        ao4_strip_end_(s);
    }
}

/* The room as a scene. */
static inline void ao4_build_room(ao4_scene_t *s) {
    unsigned pass, k, i;

    memset(s, 0, sizeof *s);
    for (pass = 0; pass < 3u; pass++) {                       /* the floors, the walls, the rest */
        for (k = 0; k < ROOM_OBJS; k++) {
            const int surf = room_obj[k].surf;
            const unsigned kind = surf < 0 ? 0u : room_surf[surf].kind;

            if ((pass == 0u && kind == ROOM_FLOOR) || (pass == 1u && kind == ROOM_WALL) || (pass == 2u && kind == 0u)) ao4_room_obj_(s, k);
        }
        if (pass == 0u) { s->ground_vn = s->vn; s->ground_strips = s->strips; }
        if (pass == 1u) s->surf_strips = s->strips;
    }
    for (i = 0; i < ROOM_CV; i++) { s->cv[i][0] = room_cv[i][0]; s->cv[i][1] = room_cv[i][1]; s->cv[i][2] = room_cv[i][2]; }
    s->cvn = ROOM_CV;
    for (k = 0; k < ROOM_OBJS; k++)
        for (i = 0; i < room_obj[k].ctn; i++) {
            const unsigned t = room_obj[k].ct0 + i;

            s->ct_open[s->ctn] = (uint8_t)((room_obj[k].flags & ROOM_OPEN) != 0);
            ao4_ctri_(s, room_ct[t][0], room_ct[t][1], room_ct[t][2]);
        }
    ao4_sun_(s, 0.45f, 0.80f, -0.55f);
    s->no_sun_shadow = 1; s->cam_rh = 1;
    s->cam_angle = room_cam_angle; s->cam_swing = 0.45f;
    s->cam_radius = room_cam_radius * AO4_ROOM_CAM; s->cam_height = room_eye[1] * AO4_ROOM_CAM; s->cam_tx = room_target[0]; s->cam_tz = room_target[2];
}

/* Corner c (0 .. 3) of surface i: o, o + du, o + du + dv, o + dv; its lightmap coordinates are (0, 0), (1, 0), (1, 1), (0, 1). */
static inline void ao4_room_quad(unsigned i, unsigned c, float p[3], float uv[2]) {
    const room_surf_t *f = &room_surf[i];
    const float a = (c == 1u || c == 2u) ? 1.0f : 0.0f, b = c >= 2u ? 1.0f : 0.0f;

    p[0] = f->o[0] + f->du[0] * a + f->dv[0] * b; p[1] = f->o[1] + f->du[1] * a + f->dv[1] * b; p[2] = f->o[2] + f->du[2] * a + f->dv[2] * b;
    uv[0] = a; uv[1] = b;
}

/* A surface's two colours for the lightmap's header (out = texel x base + offset): the ambient, and the sun's share
   at its one normal. */
static inline void ao4_room_colours(const ao4_scene_t *s, unsigned i, unsigned look, uint32_t *base, uint32_t *offset) {
    const room_surf_t *f = &room_surf[i];
    const float ndl = sh4der_dot3_(f->n, s->sun), sun = ao4_sunc[look] * (ndl > 0.0f ? ndl : 0.0f), amb = ao4_amb[look];
    const float *t = ao4_tint[room_tint[f->v0] & 3u];
    const float g = look == AO4_CLAY ? AO4_CLAY_GREY : 1.0f;
    float c[3];
    unsigned k;
    uint32_t b = 0xff000000u, o = 0xff000000u;

    for (k = 0; k < 3u; k++) {
        c[k] = look == AO4_COLOUR ? t[k] : g;
        b |= (uint32_t)(sh4der_sat_(c[k] * amb) * 255.0f + 0.5f) << (16u - 8u * k);
        o |= (uint32_t)(sh4der_sat_(c[k] * sun) * 255.0f + 0.5f) << (16u - 8u * k);
    }
    *base = b; *offset = o;
}

/* Surface i's lightmap as RGB565 greys into px (w x h): the AO byte, doubled occlusion under `exaggerate`. */
static inline void ao4_room_lm565(unsigned i, int exaggerate, uint16_t *px) {
    const unsigned n = (unsigned)room_surf[i].w * room_surf[i].h;
    unsigned k;

    for (k = 0; k < n; k++) {
        const unsigned a = (unsigned)(ao4_open(room_lm[i][k], exaggerate) * 255.0f + 0.5f);

        px[k] = (uint16_t)(((a >> 3) << 11) | ((a >> 2) << 5) | (a >> 3));
    }
}

#endif /* AO4_ROOM_H */
