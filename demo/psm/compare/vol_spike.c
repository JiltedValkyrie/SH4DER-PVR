/* SH4DER-PVR: the modifier volume spike, the Dreamcast test program

   demo/psm/compare/vol_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Shadow technique: John Brooks (the World Series Baseball 2K2 renderer): shadows as PVR modifier volumes extruded
   from the caster's own triangles. Used with his permission, credit required. This program is our own code.

   A test program for a real console. Spike c1 showed one modifier volume marks exactly the pixels inside it. This one
   asks what a town of them needs. It runs its parts once, shows a live view (START leaves it), then result pages
   (A next, B back); the same lines go to the serial line ("vol:").

     1 RULES   in screen space over a 256 x 256 square, read back and counted:
               R  what "inside" is: one volume face in front of the surface, one behind, a closed volume in front,
                  one behind, one round the surface
               N  a near face very far toward the eye (1 / w of 1000 and 1e7): can a volume cut at the near plane be
                  closed there
               O  two boxes that overlap, as two volumes and as one (does a pixel in both go lit again)
               S  a surface exactly on a volume's near face and on its far face, flat and sloped (the caster's own
                  faces)
               K  what is marked: a gouraud surface, a textured one, one without the shadow bit, a translucent one
                  under a volume in the opaque list and in the translucent list, the two-parameter mode, scale 0.25
     2 TOWN    a textured ground (512 triangles) with 1, 8, 32, 128 boxes, each with a volume to the ground along the
               sun. Per count: SH-4 time to build and to send the volumes, their triangles, the vertex buffer used,
               the PVR's render time with and without them, the pixels they darkened. A count is tried only if the
               one before came back.
     3 CASTER  a 600-triangle sphere: the silhouette volume against the per-triangle one, build and send apart.
     4 TARGET  the volume pass into a render target instead of the screen, copied to RAM by PVR-DMA, read per vertex
               by the SH-4: is it exact there (c1's 11,008 pixel test), what it costs at 160 x 120 and 320 x 240.

   The live view: left / right the box count; X the casters take shadow too; A the caps on the lit or the dark faces;
   Y the cap moved 3 cm along the light; B the sphere's volume per triangle. Squares in the top left corner show the
   state: white = count step, green = casters take shadow, red = caps on lit faces, blue = cap moved, yellow = per
   triangle.

   A volume with a vertex nearer than the near plane is not sent (counted as "skipped"): the cut is not in this spike.
   The fault catcher (include/sh4der_fault.h) is in. Every number comes from the run that prints it; only a real console's
   run is evidence. Build: python demo/psm/compare/build.py --spike vol.
*/

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "cmp_model.h"
#include "vol_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 120u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static sh4der_fault_crumbs_t crumbs;

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next. */
static void say(const char *fmt, ...) {
    char big[200];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("vol: %s\n", big);
    for (left = strlen(big); nlines < LINES_MAX; ) {
        const int cont = s != big;
        const size_t room = cont ? COLS - 3u : COLS, n = left < room ? left : room;

        snprintf(lines[nlines++], COLS + 1u, "%s%.*s", cont ? "   " : "", (int)n, s);
        s += n; left -= n;
        if (!left) break;
    }
}

static void page_draw(unsigned page, unsigned pages) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "PVR VOLUMES c2 p%u/%u   A next  B back", page + 1u, pages);
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

static unsigned us_of(uint64_t ns) { return (unsigned)((ns + 500u) / 1000u); }

/* ---- PVR-DMA, PVR to system memory (as the route AB spike measured it) ------------ */

#define REG32(a) (*(volatile uint32_t *)(a))
#define SB_ISTNRM   REG32(0xa05f6900)
#define SB_PDSTAP   REG32(0xa05f7c00)
#define SB_PDSTAR   REG32(0xa05f7c04)
#define SB_PDLEN    REG32(0xa05f7c08)
#define SB_PDDIR    REG32(0xa05f7c0c)
#define SB_PDTSEL   REG32(0xa05f7c10)
#define SB_PDEN     REG32(0xa05f7c14)
#define SB_PDST     REG32(0xa05f7c18)
#define SB_PDAPRO   REG32(0xa05f7c80)
#define ISTNRM_PVR_DMA (1u << 11)
#define PD_TIMEOUT_NS 50000000ull

/* `bytes` (a multiple of 32) of video memory at `mem` into `dst` (32-byte aligned RAM). Returns the time in ns, 0
   when the download did not end inside the timeout. The end is seen by polling. */
static uint64_t pd_download(pvr_ptr_t mem, void *dst, uint32_t bytes) {
    uint64_t t0, t1;
    int done;

    dcache_purge_range((uintptr_t)dst, bytes);
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    SB_PDAPRO = 0x6702007fu;
    SB_PDSTAP = 0x04000000u | ((uint32_t)(uintptr_t)mem & 0x00ffffffu);
    SB_PDSTAR = (uint32_t)(uintptr_t)dst & 0x1fffffe0u;
    SB_PDLEN = bytes;
    SB_PDDIR = 1;
    SB_PDTSEL = 0;
    SB_PDEN = 1;
    t0 = timer_ns_gettime64();
    SB_PDST = 1;
    do { t1 = timer_ns_gettime64(); } while ((SB_PDST & 1u) && t1 - t0 < PD_TIMEOUT_NS);
    done = !(SB_PDST & 1u);
    if (!done) SB_PDST = 0;
    SB_PDEN = 0;
    SB_ISTNRM = ISTNRM_PVR_DMA;
    dcache_inval_range((uintptr_t)dst, bytes);
    return done ? (t1 - t0 ? t1 - t0 : 1u) : 0u;
}

/* ---- video memory, headers ---------------------------------------------------------- */

#define RTT_W 320u
#define RTT_ROWS 264u                      /* rows allocated: the largest target is 258 rows */
#define RTT_BYTES (RTT_W * RTT_ROWS * 2u)
#define PREFILL 0x1234u                    /* put in the target by the SH-4 before a pass: a pixel still holding it was never written */

static pvr_ptr_t check_mem, white_mem, rtt_mem;
static uint16_t check_px[64 * 64] __attribute__((aligned(32)));
static uint16_t white_px[8 * 8] __attribute__((aligned(32)));
static uint16_t ram_img[RTT_W * RTT_ROWS] __attribute__((aligned(32)));        /* the target in RAM */
static uint8_t img[CMP_N * CMP_N], model[CMP_N * CMP_N];

static pvr_poly_hdr_t h_plain, h_recv, h_ground, h_tex_recv, h_tr_recv;
static pvr_poly_mod_hdr_t h_two;
static pvr_mod_hdr_t m_op, m_op_last, m_tr, m_tr_last;

static void recv_bits(pvr_poly_cxt_t *c) {  /* this polygon takes the cheap shadow */
    c->gen.modifier_mode = PVR_MODIFIER_CHEAP_SHADOW;
    c->fmt.modifier = PVR_MODIFIER_ENABLE;
}

static int load(void) {
    pvr_poly_cxt_t c;
    unsigned x, y;

    check_mem = pvr_mem_malloc(sizeof check_px);
    white_mem = pvr_mem_malloc(sizeof white_px);
    rtt_mem = pvr_mem_malloc(RTT_BYTES);
    if (!check_mem || !white_mem || !rtt_mem) return 0;
    for (y = 0; y < 64u; y++) for (x = 0; x < 64u; x++) check_px[y * 64u + x] = ((x ^ y) & 32u) ? 0xffffu : 0xef5du;   /* white and grey 232 */
    for (x = 0; x < 64u; x++) white_px[x] = 0xffffu;
    pvr_txr_load(check_px, check_mem, sizeof check_px);
    pvr_txr_load(white_px, white_mem, sizeof white_px);

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY); c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_plain, &c);
    recv_bits(&c);
    pvr_poly_compile(&h_recv, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 64, 64, check_mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; recv_bits(&c);
    pvr_poly_compile(&h_ground, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 8, 8, white_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE; recv_bits(&c);
    pvr_poly_compile(&h_tex_recv, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY); c.gen.culling = PVR_CULLING_NONE; recv_bits(&c);
    pvr_poly_compile(&h_tr_recv, &c);
    pvr_poly_cxt_col_mod(&c, PVR_LIST_OP_POLY); c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_mod_compile(&h_two, &c);
    pvr_mod_compile(&m_op, PVR_LIST_OP_MOD, PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&m_op_last, PVR_LIST_OP_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&m_tr, PVR_LIST_TR_MOD, PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&m_tr_last, PVR_LIST_TR_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);
    return 1;
}

/* ---- passes --------------------------------------------------------------------------- */

static uint32_t render_addr;
static uint64_t last_render_ns;
static size_t last_vtx_bytes;

static int scene_open(pvr_ptr_t mem, unsigned w, unsigned h) {
    pvr_wait_ready();
    if (mem) return pvr_scene_begin_rtt(mem, w, h, w) >= 0;
    pvr_scene_begin();
    return 1;
}

/* Close the pass and wait for the render to start and to end. 1 when both happened; the render time and the vertex
   buffer use are then in last_render_ns and last_vtx_bytes. */
static int scene_close(void) {
    pvr_stats_t st;
    unsigned t;

    pvr_scene_finish();
    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) return 0;
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) return 0;
    render_addr = PVR_GET(PVR_RENDER_ADDR);
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    last_vtx_bytes = st.vtx_buffer_used;
    return 1;
}

static const volatile uint16_t *frame_px(void) { return (const volatile uint16_t *)(uintptr_t)(0xa5000000u | (render_addr & 0x00ffffffu)); }

/* The 256 x 256 square as 8 bits of green, from a picture `stride` pixels wide. */
static void square_from(const volatile uint16_t *fb, unsigned stride) {
    unsigned x, y;

    for (y = 0; y < CMP_N; y++) {
        for (x = 0; x < CMP_N; x++) {
            const unsigned g = (fb[y * stride + x] >> 5) & 63u;

            img[y * CMP_N + x] = (uint8_t)((g << 2) | (g >> 4));
        }
    }
}

/* ---- triangles -------------------------------------------------------------------------- */

static void quad(const void *hdr, size_t hdr_size, float x0, float y0, float x1, float y1, uint32_t col, float zl, float zr, int textured) {
    pvr_vertex_t v;
    unsigned i;

    pvr_prim((void *)hdr, hdr_size);
    memset(&v, 0, sizeof v);
    v.argb = col;
    for (i = 0; i < 4u; i++) {
        const int right = (i & 2u) != 0, top = (i & 1u) != 0;

        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = right ? x1 : x0; v.y = top ? y0 : y1; v.z = right ? zr : zl;
        if (textured) { v.u = right ? 1.0f : 0.0f; v.v = top ? 0.0f : 1.0f; }
        pvr_prim(&v, sizeof v);
    }
}

/* The same quad for the two-parameter mode: colour c0 outside a volume, c1 inside. */
static void quad_two(float x0, float y0, float x1, float y1, uint32_t c0, uint32_t c1, float z) {
    pvr_vertex_pcm_t v;
    unsigned i;

    pvr_prim(&h_two, sizeof h_two);
    memset(&v, 0, sizeof v);
    v.argb0 = c0; v.argb1 = c1; v.z = z;
    for (i = 0; i < 4u; i++) {
        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 2u) ? x1 : x0; v.y = (i & 1u) ? y0 : y1;
        pvr_prim(&v, sizeof v);
    }
}

/* One volume: its triangles are collected, then sent with the last under the "last polygon" header. */
static float vt[VOL_OUT_MAX][9];
static unsigned nvt;

static void vol_tri(const float a[3], const float b[3], const float c[3]) {
    if (nvt >= VOL_OUT_MAX) return;
    memcpy(vt[nvt], a, 12u); memcpy(vt[nvt] + 3, b, 12u); memcpy(vt[nvt] + 6, c, 12u);
    nvt++;
}

static unsigned vol_send(int translucent) {
    pvr_modifier_vol_t m;
    const unsigned n = nvt;
    unsigned i;

    if (!n) return 0;
    memset(&m, 0, sizeof m);
    m.flags = PVR_CMD_VERTEX_EOL;
    if (n > 1u) pvr_prim(translucent ? &m_tr : &m_op, sizeof m_op);
    for (i = 0; i < n; i++) {
        if (i == n - 1u) pvr_prim(translucent ? &m_tr_last : &m_op_last, sizeof m_op_last);
        memcpy(&m.ax, vt[i], 36u);
        pvr_prim(&m, sizeof m);
    }
    nvt = 0;
    return n;
}

/* ---- 1. the rules ------------------------------------------------------------------------- */

enum { S_GOURAUD = 0, S_TEX = 1, S_PLAIN = 2, S_TR = 3, S_TWO = 4 };

typedef struct vface {
    float x0, x1, za, zb;                  /* a rectangle x0 .. x1, y 64 .. 192, 1 / w za at x0 and zb at x1 */
    int close;                             /* the last face of its volume */
} vface_t;

typedef struct flat {
    const char *name;
    int surf, tr_vol;                      /* the surface's kind; the volume in the translucent modifier list */
    float zl, zr, scale;                   /* the surface's 1 / w left and right; the cheap shadow scale */
    int want;                              /* pixels marked if "inside" is the parity of faces in front; -1: no model */
    unsigned nf;
    vface_t f[4];
} flat_t;

static const flat_t flats[] = {
    { "R1 one face in front", S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 16384, 1, { { 64, 192, 0.7f, 0.7f, 1 } } },
    { "R2 one face behind",   S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 0, 1, { { 64, 192, 0.3f, 0.3f, 1 } } },
    { "R3 closed in front",   S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 0, 2, { { 64, 192, 0.9f, 0.9f, 0 }, { 64, 192, 0.7f, 0.7f, 1 } } },
    { "R4 closed behind",     S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 0, 2, { { 64, 192, 0.3f, 0.3f, 0 }, { 64, 192, 0.1f, 0.1f, 1 } } },
    { "R5 round surface",     S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 16384, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "N1 near face 1/w 1000", S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 16384, 2, { { 64, 192, 1000.0f, 1000.0f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "N2 near face 1/w 1e7", S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 16384, 2, { { 64, 192, 1e7f, 1e7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "O1 overlap, 2 volumes", S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 24576, 4,
      { { 32, 160, 0.7f, 0.7f, 0 }, { 32, 160, 0.3f, 0.3f, 1 }, { 96, 224, 0.7f, 0.7f, 0 }, { 96, 224, 0.3f, 0.3f, 1 } } },
    { "O2 overlap, 1 volume", S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 16384, 4,
      { { 32, 160, 0.7f, 0.7f, 0 }, { 32, 160, 0.3f, 0.3f, 0 }, { 96, 224, 0.7f, 0.7f, 0 }, { 96, 224, 0.3f, 0.3f, 1 } } },
    { "S1 on near face",      S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, -1, 2, { { 64, 192, 0.5f, 0.5f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "S2 on far face",       S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, -1, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.5f, 0.5f, 1 } } },
    { "S3 slope, on near",    S_GOURAUD, 0, 0.2f, 0.8f, 0.5f, -1, 2, { { 64, 192, 0.35f, 0.65f, 0 }, { 64, 192, 0.2f, 0.5f, 1 } } },
    { "S4 slope, on far",     S_GOURAUD, 0, 0.2f, 0.8f, 0.5f, -1, 2, { { 64, 192, 0.5f, 0.8f, 0 }, { 64, 192, 0.35f, 0.65f, 1 } } },
    { "K1 gouraud",           S_GOURAUD, 0, 0.5f, 0.5f, 0.5f, 16384, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "K2 textured",          S_TEX,     0, 0.5f, 0.5f, 0.5f, 16384, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "K3 no shadow bit",     S_PLAIN,   0, 0.5f, 0.5f, 0.5f, 0, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "K4 transl, opaque vol", S_TR,     0, 0.5f, 0.5f, 0.5f, -1, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "K5 transl, transl vol", S_TR,     1, 0.5f, 0.5f, 0.5f, -1, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "K6 two-parameter",     S_TWO,     0, 0.5f, 0.5f, 1.0f, 16384, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
    { "K7 scale 0.25",        S_GOURAUD, 0, 0.5f, 0.5f, 0.25f, 16384, 2, { { 64, 192, 0.7f, 0.7f, 0 }, { 64, 192, 0.3f, 0.3f, 1 } } },
};

static void flat_volumes(const flat_t *t) {
    unsigned i;

    for (i = 0; i < t->nf; i++) {
        const vface_t *f = &t->f[i];
        const float a[3] = { f->x0, 64.0f, f->za }, b[3] = { f->x0, 192.0f, f->za }, c[3] = { f->x1, 64.0f, f->zb }, d[3] = { f->x1, 192.0f, f->zb };

        vol_tri(a, b, c);
        vol_tri(c, b, d);
        if (f->close) vol_send(t->tr_vol);
    }
}

static void flat_run(const flat_t *t) {
    unsigned i, n = 0, in, out;
    char want[16];

    step(t->name);
    pvr_set_shadow_scale(t->surf != S_TWO, t->scale);            /* the two-parameter mode is the cheap shadow switched off */
    if (!scene_open(NULL, 0u, 0u)) { say("%s NOT DRAWN", t->name); return; }
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h_plain, sizeof h_plain, 0.0f, 0.0f, 640.0f, 480.0f, 0xff000000u, 0.01f, 0.01f, 0);        /* a black back wall */
    if (t->surf == S_GOURAUD) quad(&h_recv, sizeof h_recv, 0.0f, 0.0f, 256.0f, 256.0f, 0xffffffffu, t->zl, t->zr, 0);
    else if (t->surf == S_TEX) quad(&h_tex_recv, sizeof h_tex_recv, 0.0f, 0.0f, 256.0f, 256.0f, 0xffffffffu, t->zl, t->zr, 1);
    else if (t->surf == S_PLAIN) quad(&h_plain, sizeof h_plain, 0.0f, 0.0f, 256.0f, 256.0f, 0xffffffffu, t->zl, t->zr, 0);
    else if (t->surf == S_TWO) quad_two(0.0f, 0.0f, 256.0f, 256.0f, 0xffffffffu, 0xff004000u, t->zl);
    pvr_list_finish();
    if (!t->tr_vol) { pvr_list_begin(PVR_LIST_OP_MOD); flat_volumes(t); pvr_list_finish(); }
    if (t->surf == S_TR) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        quad(&h_tr_recv, sizeof h_tr_recv, 0.0f, 0.0f, 256.0f, 256.0f, 0xffffffffu, t->zl, t->zr, 0);
        pvr_list_finish();
    }
    if (t->tr_vol) { pvr_list_begin(PVR_LIST_TR_MOD); flat_volumes(t); pvr_list_finish(); }
    if (!scene_close()) { say("%s NO RENDER END", t->name); return; }
    square_from(frame_px(), 640u);
    out = img[16u * CMP_N + 16u];                                /* a pixel no volume covers */
    in = img[128u * CMP_N + 128u];                               /* the middle of every box */
    for (i = 0; i < CMP_N * CMP_N; i++) n += (img[i] > out ? img[i] - out : out - img[i]) > 16u;
    if (t->want < 0) strcpy(want, "?"); else snprintf(want, sizeof want, "%d", t->want);
    say("%s: %u (%s) %u/%u", t->name, n, want, in, out);
}

/* ---- the town ------------------------------------------------------------------------------ */

#define GRID 16u                           /* ground cells a side, 2 m each */
#define BOXES_MAX 128u

static float sun[3] = { 0.5f, 0.8f, -0.3f };                     /* toward the light; made unit at start */
static float sph_p[VOL_MAX_V][3], box_unit[8][3], box_p[8][3];
static uint16_t sph_t[VOL_MAX_T][3], sph_e[VOL_MAX_E][4], box_t[12][3], box_e[18][4];
static vol_mesh_t sph = { sph_p, sph_t, sph_e, 0, 0, 0 }, box = { box_p, box_t, box_e, 0, 0, 0 };
static uint16_t out_idx[VOL_OUT_MAX][3];
static uint8_t side[VOL_MAX_T];
static float scr[2u * VOL_MAX_V][3];
static float gscr[(GRID + 1u) * (GRID + 1u)][3];
static uint8_t gok[(GRID + 1u) * (GRID + 1u)];

typedef struct town {
    unsigned n;                            /* boxes */
    int sphere, vols;                      /* draw the sphere; send the volumes */
    int recv, cap_lit, per_tri, flat;      /* casters take shadow; caps on the lit faces; the sphere's volume per triangle;
                                              flat = the picture for the SH-4: ground white, casters blue, no texture */
    float bias;                            /* the near cap moved this far along the light, into the caster */
    unsigned marks;                        /* the live view's state squares (bit set = drawn) */
} town_t;

typedef struct town_res {
    uint64_t build_ns, send_ns, render_ns;
    unsigned mod_tris, skipped;
    size_t vtx_bytes;
} town_res_t;

static void box_place(unsigned k) {
    static const uint8_t col[16] = { 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15, 0 }, row[8] = { 2, 3, 1, 4, 0, 5, 6, 7 };
    const float x = -15.4f + 2.0f * (float)col[k % 16u], z = 4.0f + 3.5f * (float)row[k / 16u], h = 1.0f + 0.6f * (float)(k % 3u);
    unsigned i;

    for (i = 0; i < 8u; i++) { box_p[i][0] = x + 0.8f * box_unit[i][0]; box_p[i][1] = h * box_unit[i][1]; box_p[i][2] = z + 0.8f * box_unit[i][2]; }
}

/* A caster's own triangles on the opaque list: flat shaded by the sun, or blue for the SH-4's picture. */
static void mesh_draw(const vol_mesh_t *m, const vol_cam_t *cam, const town_t *t) {
    pvr_vertex_t v;
    unsigned i, k, ok = 1;

    for (i = 0; i < m->nv; i++) ok &= (unsigned)vol_project(cam, m->p[i], scr[i]);
    if (!ok) return;
    pvr_prim(t->recv && !t->flat ? &h_recv : &h_plain, sizeof h_plain);
    memset(&v, 0, sizeof v);
    for (i = 0; i < m->nt; i++) {
        float n[3], d;
        unsigned g;

        vol_tri_normal(m, i, n);
        vol_unit(n);
        d = vol_dot(n, sun);
        g = (unsigned)(255.0f * (0.6f + 0.4f * (d > 0.0f ? d : 0.0f)));
        v.argb = t->flat ? 0xff0000ffu : 0xff000000u | (g << 16) | (g << 8) | (g * 7u / 8u);
        for (k = 0; k < 3u; k++) {
            const float *s = scr[m->tri[i][k]];

            v.flags = k == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = s[0]; v.y = s[1]; v.z = s[2];
            pvr_prim(&v, sizeof v);
        }
    }
}

/* A caster's volume on the opaque modifier list: build (timed), then transform and send (timed). */
static void mesh_volume(const vol_mesh_t *m, const vol_cam_t *cam, const town_t *t, int per_tri, town_res_t *r) {
    const float b = t->cap_lit ? -t->bias : t->bias;             /* dark caps move toward the light, lit caps away */
    const uint64_t t0 = timer_ns_gettime64();
    uint64_t t1;
    unsigned i, n, ok = 1;

    vol_facing(m, sun, side);
    n = per_tri ? vol_extrude_tri(m, side, t->cap_lit, out_idx) : vol_extrude_sil(m, side, t->cap_lit, out_idx, NULL);
    t1 = timer_ns_gettime64();
    r->build_ns += t1 - t0;
    for (i = 0; i < m->nv; i++) {
        const float near[3] = { m->p[i][0] + sun[0] * b, m->p[i][1] + sun[1] * b, m->p[i][2] + sun[2] * b };
        float far[3];

        vol_far_plane(m->p[i], sun, -0.2f, far);
        ok &= (unsigned)vol_project(cam, near, scr[i]);
        ok &= (unsigned)vol_project(cam, far, scr[i + m->nv]);
    }
    if (!ok) { r->skipped++; return; }
    for (i = 0; i < n; i++) vol_tri(scr[out_idx[i][0]], scr[out_idx[i][1]], scr[out_idx[i][2]]);
    r->mod_tris += vol_send(0);
    r->send_ns += timer_ns_gettime64() - t1;
}

static void ground_draw(const vol_cam_t *cam, int flat) {
    pvr_vertex_t v;
    unsigned x, z, k;

    for (z = 0; z <= GRID; z++) {
        for (x = 0; x <= GRID; x++) {
            const float p[3] = { -16.0f + 2.0f * (float)x, 0.0f, 2.0f * (float)z };

            gok[z * (GRID + 1u) + x] = (uint8_t)vol_project(cam, p, gscr[z * (GRID + 1u) + x]);
        }
    }
    pvr_prim(flat ? &h_recv : &h_ground, sizeof h_recv);
    memset(&v, 0, sizeof v);
    v.argb = 0xffffffffu;
    for (z = 0; z < GRID; z++) {
        for (x = 0; x < GRID; x++) {
            static const uint8_t c[6][2] = { {0,0}, {0,1}, {1,0}, {1,0}, {0,1}, {1,1} };
            const unsigned i0 = z * (GRID + 1u) + x;

            if (!(gok[i0] & gok[i0 + 1u] & gok[i0 + GRID + 1u] & gok[i0 + GRID + 2u])) continue;
            for (k = 0; k < 6u; k++) {
                const float *s = gscr[i0 + c[k][0] + c[k][1] * (GRID + 1u)];

                v.flags = (k % 3u) == 2u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = s[0]; v.y = s[1]; v.z = s[2];
                v.u = (float)(x + c[k][0]); v.v = (float)(z + c[k][1]);
                pvr_prim(&v, sizeof v);
            }
        }
    }
}

/* One frame of the town to the screen or to a w x h target. 1 when the render came back. */
static int town_frame(const town_t *t, const vol_cam_t *cam, pvr_ptr_t mem, unsigned w, unsigned h, town_res_t *r) {
    static const uint32_t mark_col[8] = { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xff00ff00u, 0xffff0000u, 0xff0000ffu, 0xffffff00u };
    unsigned k;

    memset(r, 0, sizeof *r);
    sh4der_fault_beat(&crumbs);
    if (!scene_open(mem, w, h)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    ground_draw(cam, t->flat);
    for (k = 0; k < t->n; k++) { box_place(k); mesh_draw(&box, cam, t); }
    if (t->sphere) mesh_draw(&sph, cam, t);
    for (k = 0; k < 8u; k++)
        if (t->marks & (1u << k)) quad(&h_plain, sizeof h_plain, 8.0f + 14.0f * (float)k, 8.0f, 18.0f + 14.0f * (float)k, 18.0f, mark_col[k], 50.0f, 50.0f, 0);
    pvr_list_finish();
    if (t->vols) {
        pvr_list_begin(PVR_LIST_OP_MOD);
        for (k = 0; k < t->n; k++) { box_place(k); mesh_volume(&box, cam, t, 0, r); }
        if (t->sphere) mesh_volume(&sph, cam, t, t->per_tri, r);
        pvr_list_finish();
    }
    if (!scene_close()) return 0;
    r->render_ns = last_render_ns;
    r->vtx_bytes = last_vtx_bytes;
    return 1;
}

static void cam_for(vol_cam_t *cam, unsigned w, unsigned h) {
    static const float eye[3] = { 0.0f, 6.0f, -6.0f }, target[3] = { 0.0f, 0.0f, 10.0f };

    vol_cam_look(cam, eye, target, 400.0f * (float)w / 640.0f, 0.5f * (float)w, 0.5f * (float)h);
}

/* Pixels of the 640 x 480 frame darker than anything lit in the town (a shadowed ground pixel, or a shadowed caster). */
static unsigned dark_px(void) {
    const volatile uint16_t *fb = frame_px();
    unsigned i, n = 0;

    for (i = 0; i < 640u * 480u; i++) n += ((fb[i] >> 5) & 63u) < 34u;
    return n;
}

/* ---- 2. the town's cost ----------------------------------------------------------------- */

static void town_cost(void) {
    static const unsigned counts[4] = { 1u, 8u, 32u, 128u };
    town_t t;
    town_res_t off, on;
    vol_cam_t cam;
    unsigned i, d0, d1;

    cam_for(&cam, 640u, 480u);
    memset(&t, 0, sizeof t);
    pvr_set_shadow_scale(true, 0.5f);
    say("2 TOWN boxes: SH-4 us, PVR us, from this run");
    for (i = 0; i < 4u; i++) {
        char name[24];

        snprintf(name, sizeof name, "2 town %u", counts[i]);
        step(name);
        t.n = counts[i];
        t.vols = 0;
        if (!town_frame(&t, &cam, NULL, 0u, 0u, &off) || !town_frame(&t, &cam, NULL, 0u, 0u, &off)) { say("N=%u: no render end, stop", t.n); break; }
        d0 = dark_px();
        t.vols = 1;
        if (!town_frame(&t, &cam, NULL, 0u, 0u, &on) || !town_frame(&t, &cam, NULL, 0u, 0u, &on)) { say("N=%u volumes: no render end, stop", t.n); break; }
        d1 = dark_px();
        say("N=%u: build %u send %u us, %u tri, skip %u", t.n, us_of(on.build_ns), us_of(on.send_ns), on.mod_tris, on.skipped);
        say("   TA %u/%u B, rnd %u/%u us, dark %u px", (unsigned)on.vtx_bytes, (unsigned)off.vtx_bytes, us_of(on.render_ns), us_of(off.render_ns), d1 - d0);
        if (on.render_ns > 100000000ull) { say("   over 100 ms: stop"); break; }
    }
}

/* ---- 3. the 600-triangle caster ------------------------------------------------------------ */

static void caster_cost(void) {
    town_t t;
    town_res_t r, none;
    vol_cam_t cam;
    uint64_t lo[2] = { ~0ull, ~0ull }, t0, dt;
    unsigned n[2] = { 0, 0 }, i, k, sil = 0;

    step("3 caster");
    cam_for(&cam, 640u, 480u);
    for (k = 0; k < 2u; k++) {
        for (i = 0; i < 16u; i++) {
            t0 = timer_ns_gettime64();
            vol_facing(&sph, sun, side);
            n[k] = k ? vol_extrude_tri(&sph, side, 0, out_idx) : vol_extrude_sil(&sph, side, 0, out_idx, &sil);
            dt = timer_ns_gettime64() - t0;
            if (dt < lo[k]) lo[k] = dt;
        }
    }
    say("3 CASTER sphere %u tri %u vtx, %u silhouette edges", sph.nt, sph.nv, sil);
    say("build (min of 16): silhouette %u us (%u tri)", us_of(lo[0]), n[0]);
    say("   per-tri %u us (%u tri)", us_of(lo[1]), n[1]);
    memset(&t, 0, sizeof t);
    t.sphere = 1;
    if (!town_frame(&t, &cam, NULL, 0u, 0u, &none) || !town_frame(&t, &cam, NULL, 0u, 0u, &none)) { say("3 no render end"); return; }
    for (k = 0; k < 2u; k++) {
        t.vols = 1; t.per_tri = (int)k;
        if (!town_frame(&t, &cam, NULL, 0u, 0u, &r) || !town_frame(&t, &cam, NULL, 0u, 0u, &r)) { say("3 %s: no render end", k ? "per-tri" : "silhouette"); return; }
        say("%s: %u tri, send %u us, TA %u B", k ? "per-tri" : "silhouette", r.mod_tris, us_of(r.send_ns), (unsigned)(r.vtx_bytes - none.vtx_bytes));
        say("   rnd %u us (%u without), dark %u px", us_of(r.render_ns), us_of(none.render_ns), dark_px());
    }
}

/* ---- 4. the volume pass in a render target -------------------------------------------------- */

static void prefill(void) {
    volatile uint16_t *p = (volatile uint16_t *)rtt_mem;
    unsigned i;

    for (i = 0; i < RTT_W * RTT_ROWS; i++) p[i] = PREFILL;
}

/* c1's volume test, in a 256-wide target of 258 rows (the PVR holds back the end of the last row drawn until the
   next target render: route AB, hardware result s3; so two rows are spare). */
static void target_exact(void) {
    unsigned x, y, n[2] = { 0, 0 }, diff[2] = { 0, 0 }, unwritten = 0, k, x0 = 999u, x1 = 0u, y0 = 999u, y1 = 0u;
    const volatile uint16_t *vp = (const volatile uint16_t *)rtt_mem;
    uint64_t dma;

    step("4 target exact");
    prefill();
    pvr_set_shadow_scale(true, 0.5f);
    if (!scene_open(rtt_mem, 256u, 258u)) { say("4 exact: target refused"); return; }
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&h_plain, sizeof h_plain, 0.0f, 0.0f, 256.0f, 258.0f, 0xff000000u, 0.01f, 0.01f, 0);
    quad(&h_recv, sizeof h_recv, 0.0f, 0.0f, 256.0f, 256.0f, 0xffffffffu, CMP_VOL_Z0, CMP_VOL_Z1, 0);
    quad(&h_plain, sizeof h_plain, 0.0f, 0.0f, 8.0f, 8.0f, 0xff0000ffu, 5.0f, 5.0f, 0);          /* a blue mark: which corner is the top left */
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_OP_MOD);
    {
        const float lo = (float)CMP_VOL_LO, hi = (float)CMP_VOL_HI;
        const float a[3] = { lo, lo, CMP_VOL_ZB }, b[3] = { lo, hi, CMP_VOL_ZB }, c[3] = { hi, lo, CMP_VOL_ZB }, d[3] = { hi, hi, CMP_VOL_ZB };
        const float e[3] = { lo, lo, CMP_VOL_ZA }, f[3] = { lo, hi, CMP_VOL_ZA }, g[3] = { hi, lo, CMP_VOL_ZA }, h[3] = { hi, hi, CMP_VOL_ZA };

        vol_tri(a, b, c); vol_tri(c, b, d); vol_tri(e, f, g); vol_tri(g, f, h);
        vol_send(0);
    }
    pvr_list_finish();
    if (!scene_close()) { say("4 exact: NO RENDER END"); return; }
    dma = pd_download(rtt_mem, ram_img, 256u * 256u * 2u);
    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) model[y * CMP_N + x] = (uint8_t)cmp_volume_pixel(x, y);
    for (k = 0; k < 2u; k++) {                                   /* 0: video memory read by the SH-4; 1: the DMA's copy */
        if (k && !dma) break;
        square_from(k ? (const volatile uint16_t *)ram_img : vp, 256u);
        for (y = 0; y < CMP_N; y++) {
            for (x = 8; x < CMP_N; x++) {
                const int sh = img[y * CMP_N + x] > 60u && img[y * CMP_N + x] < 190u;

                diff[k] += (unsigned)(sh != model[y * CMP_N + x]);
                if (!sh) continue;
                n[k]++;
                if (k) continue;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    for (x = 0; x < 256u * 256u; x++) unwritten += vp[x] == PREFILL;
    say("4 TARGET 256x258: %u px in shadow (model 11008)", n[0]);
    say("   %u differ, box x%u-%u y%u-%u (85-170 64-191)", diff[0], x0, x1, y0, y1);
    say("   mark at top left: %04x (001f = yes); unwritten px in 256 rows: %u", (unsigned)vp[4u * 256u + 4u], unwritten);
    if (dma) say("   DMA copy 128 KB: %u us, %u px, %u differ", us_of(dma), n[1], diff[1]);
    else say("   DMA copy: did not end");
}

/* The town as a picture for the SH-4, in a w x (h + 2) target: render time with and without volumes, the copy to RAM,
   and a read per ground vertex. */
static void target_town(unsigned w, unsigned h) {
    town_t t;
    town_res_t off, on;
    vol_cam_t cam;
    uint64_t dma = ~0ull, d, t0, look;
    unsigned i, lit = 0, shadow = 0, caster = 0, outside = 0, other = 0;
    char name[24];

    snprintf(name, sizeof name, "4 target %ux%u", w, h);
    step(name);
    cam_for(&cam, w, h);
    memset(&t, 0, sizeof t);
    t.n = 32u; t.flat = 1;
    pvr_set_shadow_scale(true, 0.5f);
    if (!town_frame(&t, &cam, rtt_mem, w, h + 2u, &off) || !town_frame(&t, &cam, rtt_mem, w, h + 2u, &off)) { say("%ux%u: no render end", w, h); return; }
    t.vols = 1;
    if (!town_frame(&t, &cam, rtt_mem, w, h + 2u, &on) || !town_frame(&t, &cam, rtt_mem, w, h + 2u, &on)) { say("%ux%u volumes: no render end", w, h); return; }
    for (i = 0; i < 4u; i++) {
        d = pd_download(rtt_mem, ram_img, w * h * 2u);
        if (!d) { dma = 0; break; }
        if (d < dma) dma = d;
    }
    say("4 %ux%u, 32 boxes: rnd %u us (%u no vol)", w, h, us_of(on.render_ns), us_of(off.render_ns));
    if (!dma) { say("   DMA copy: did not end"); return; }
    say("   DMA %u B: %u us (min of 4)", w * h * 2u, us_of(dma));
    t0 = timer_ns_gettime64();
    for (i = 0; i < (GRID + 1u) * (GRID + 1u); i++) {            /* project the vertex with the picture's camera, read its pixel */
        const float p[3] = { -16.0f + 2.0f * (float)(i % (GRID + 1u)), 0.0f, 2.0f * (float)(i / (GRID + 1u)) };
        float s[3];
        unsigned g, b;

        if (!vol_project(&cam, p, s) || s[0] < 0.0f || s[0] >= (float)w || s[1] < 0.0f || s[1] >= (float)h) { outside++; continue; }
        g = (ram_img[(unsigned)s[1] * w + (unsigned)s[0]] >> 5) & 63u;
        b = ram_img[(unsigned)s[1] * w + (unsigned)s[0]] & 31u;
        if (g >= 48u) lit++;
        else if (g >= 24u) shadow++;
        else if (b >= 24u) caster++;
        else other++;
    }
    look = timer_ns_gettime64() - t0;
    say("   289 vertex reads %u us: lit %u shadow %u", us_of(look), lit, shadow);
    say("   caster %u outside %u other %u", caster, outside, other);
}

/* ---- the live view ---------------------------------------------------------------------- */

static void live(void) {
    static const unsigned counts[4] = { 1u, 8u, 32u, 128u };
    unsigned held = ~0u, ci = 2;
    town_t t;
    town_res_t r;
    vol_cam_t cam;

    step("live");
    cam_for(&cam, 640u, 480u);
    memset(&t, 0, sizeof t);
    t.sphere = 1; t.vols = 1;
    pvr_set_shadow_scale(true, 0.5f);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = held == ~0u ? 0u : now & ~held;

        held = now;
        if (hit & CONT_START) break;
        if ((hit & CONT_DPAD_RIGHT) && ci < 3u) ci++;
        if ((hit & CONT_DPAD_LEFT) && ci > 0u) ci--;
        if (hit & CONT_X) t.recv = !t.recv;
        if (hit & CONT_A) t.cap_lit = !t.cap_lit;
        if (hit & CONT_Y) t.bias = t.bias > 0.0f ? 0.0f : 0.03f;
        if (hit & CONT_B) t.per_tri = !t.per_tri;
        t.n = counts[ci];
        t.marks = ((2u << ci) - 1u) | (t.recv ? 16u : 0u) | (t.cap_lit ? 32u : 0u) | (t.bias > 0.0f ? 64u : 0u) | (t.per_tri ? 128u : 0u);
        crumbs.frame++;
        if (!town_frame(&t, &cam, NULL, 0u, 0u, &r)) { say("live view: a render did not end"); break; }
    }
}

/* ---- the run -------------------------------------------------------------------------- */

static void run(void) {
    static const float centre[3] = { 0.0f, 1.5f, 6.0f }, zero[3] = { 0.0f, 0.0f, 0.0f }, one[3] = { 1.0f, 1.0f, 1.0f };
    pvr_init_params_t init = pvr_default_params;
    unsigned i;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 1024 * 1024;
    init.opb_overflow_count = 7;
    say("numbers below: measured by this run");
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return; }
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    pvr_set_bg_color(0.5f, 0.7f, 1.0f);
    if (!load()) { say("video memory allocation FAIL"); pvr_shutdown(); return; }
    vid_set_dithering(false);

    vol_unit(sun);
    vol_sphere(&sph, 20u, 16u, 1.0f, centre);
    box.p = box_unit;
    vol_box(&box, zero, one);
    box.p = box_p;
    if (!vol_edges_build(&sph) || !vol_edges_build(&box)) { say("a mesh is not closed: FAIL"); pvr_shutdown(); return; }

    say("1 RULES px marked (parity model) in/out value");
    for (i = 0; i < sizeof flats / sizeof flats[0]; i++) flat_run(&flats[i]);
    town_cost();
    caster_cost();
    target_exact();
    target_town(160u, 120u);
    target_town(320u, 240u);
    say("video memory free: %u B", (unsigned)pvr_mem_available());
    live();

    step("done");
    vid_set_dithering(true);
    pvr_shutdown();
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;

    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (held == ~0u) page_draw(page, pages);
        else if (hit & CONT_A) { page = (page + 1u) % pages; page_draw(page, pages); }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; page_draw(page, pages); }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
