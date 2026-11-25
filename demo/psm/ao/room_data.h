/* SH4DER-PVR: a room as C arrays, with its baked AO (demo/psm/ao): the shared header

   demo/psm/ao/room_data.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A room for the AO demos: the living room modelled as OBJ files (demo/psm/ao/room, room_manifest.json), or the
   placeholder room built by the importer until those files are in. This header is the one thing a program includes.
   It defines the types and includes two build products from the include path:

     room_mesh.h   written by   python demo/psm/ao/room_import.py (--room demo/psm/ao/room | --placeholder) --out <dir>
     room_ao.h     written by   demo/psm/ao/tools/room_bake.c, a host program compiled with -I<dir>, run with <dir>
   (demo/psm/ao/build.py --room-only --out <dir> does both.) Compile the program with -I<dir>.

   Metres, y up, the floor's top at y 0. Plain data: no maths, no KallistiOS, nothing a frame. Include after <stdint.h>.

   ON SCREEN
     ROOM_V, room_v[i], room_n[i]   the vertices: place and unit normal. The floor and the walls are cut into a grid
                                    of ROOM_GRID metres, so a colour a vertex can hold their AO.
     room_tint[i]                   0 .. 3, a colour for a coloured look
     ROOM_T, room_t[t]              the triangles, counter-clockwise seen from outside
     room_ao_v[i]                   the baked AO a vertex: 255 open sky .. 0 closed. Multiply the AMBIENT light by it.
     ROOM_OBJS, room_obj[k]         the objects: name, v0 / vn and t0 / tn (its vertices and triangles above),
                                    cv0 / cvn and ct0 / ctn (its caster, below), surf (its surface's index or -1),
                                    flags (ROOM_OPEN: not a closed surface wound one way; ROOM_CASTER: it casts)

   THE FLAT SURFACES (the floor, each wall), for a lightmap in place of the grid
     ROOM_SURFS, room_surf[s]       obj; kind (ROOM_FLOOR, ROOM_WALL); w, h the lightmap's size; nu, nv the grid's
                                    cells and v0 its first vertex (vertex (i, j) is v0 + j * (nu + 1) + i); o, du, dv
                                    the rectangle (a corner and its two whole sides), n its normal
     room_lm[s]                     the lightmap, w x h bytes, row j at v = (j + 0.5) / h along dv, column i along du:
                                    the AO at o + du (i + 0.5) / w + dv (j + 0.5) / h. 255 open .. 0 closed.
                                    Drawn as ONE quad (the corners o, o + du, o + du + dv, o + dv; texture
                                    coordinates 0 .. 1 along du and dv) it replaces (nu + 1) x (nv + 1) vertices.
                                    A PVR texture's sides are powers of two: w and h are 64 or 128.

   THE CASTERS (for a shadow target: the sun's shadow, ../ao_layer.h's sky directions)
     ROOM_CV, room_cv[i]            caster vertices
     ROOM_CT, room_ct[t]            caster triangles, by object (room_obj[k].ct0, ctn). An object without ROOM_OPEN
                                    is closed and wound one way: one side draws its whole shadow (ao_tri_ok's cull).
                                    A wall's caster is a closed slab 0.2 m thick behind it; the floor casts nothing.

   THE BAKE'S OCCLUDERS (room_bake.c reads them; a program does not need them)
     ROOM_OT, room_ot[t][3][3]      the objects' triangles and each surface as two
     room_ot_closed[t]              1: the triangle's object is a closed solid wound outward (a ray that meets its
                                    back first started inside it)

   room_eye, room_target            where the model's own picture was taken from: a start for the camera
   room_cam_angle, room_cam_radius  the same as an angle round the target (radians, from +x toward +z) and a distance
                                    on the ground, for an orbit */

#ifndef ROOM_DATA_H
#define ROOM_DATA_H

#include <stdint.h>

#define ROOM_OPEN   1
#define ROOM_CASTER 2
#define ROOM_FLOOR  1
#define ROOM_WALL   2

typedef struct room_obj {
    char name[16];
    uint16_t v0, vn, t0, tn, cv0, cvn, ct0, ctn;
    int16_t surf;
    uint16_t flags;
} room_obj_t;

typedef struct room_surf {
    uint16_t obj, kind, w, h, nu, nv, v0;
    float o[3], du[3], dv[3], n[3];
} room_surf_t;

#include "room_mesh.h"
#ifndef ROOM_NO_AO                          /* room_bake.c itself: the mesh alone */
#include "room_ao.h"
#endif

#endif /* ROOM_DATA_H */
