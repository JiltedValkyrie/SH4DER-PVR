/* Sunset client; scene facts stay here, reusable behavior in library headers.
 * Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 */
#ifndef OCEAN_SCENE_H
#define OCEAN_SCENE_H
#include <stdint.h>
#include "sh4der_water.h"
#include "sh4der_warp.h"
#include "sh4dow_heightfield.h"
#ifndef SH4DER_CORE_NO_SH4ZAM
/* SH4ZAM-DCE: fused transform + projection, FSRRA normalize. Include path: SH4ZAM_DCE. */
#include <sh4zam_dce/shz_dce.h>
#endif
#define OCEAN_X 120
#define OCEAN_Z 96
#define OCEAN_ENV 512
#define OCEAN_SKY_X 48
#define OCEAN_SKY_Y 24
#define OCEAN_SHADOW_X 112
#define OCEAN_SHADOW_Z 144
#define OCEAN_CELL 0.60f
#define OCEAN_ORIGIN_X (-33.6f)
#define OCEAN_ORIGIN_Z (-7.2f)
#define OCEAN_FOCAL 470.0f
#define OCEAN_EYE 2.5f
/* LT / RT move the eye; the swell peaks at about 0.28 m (four swells 0.263 m + three ripples 0.0125 m), so 0.8 m keeps the eye 0.5 m over the
 * highest crest; at 8 m the mesh's far edge (2,796 m) is 1.4 px under the horizon, inside the fog that matches the sky. */
#define OCEAN_EYE_MIN 0.8f
#define OCEAN_EYE_MAX 8.0f
/* KOS background plane defaults to 0.0001. Sky must be in front of it
 * and behind the farthest water (about 1/2800). PVR depth is reciprocal. */
#define OCEAN_BACKGROUND_DEPTH 0.0001f
#define OCEAN_SKY_DEPTH 0.0002f
#define OCEAN_FOG_START 55.0f
#define OCEAN_FOG_END 520.0f

typedef struct { uint32_t flags; float x,y,z,u,v; uint32_t argb,oargb; } ocean_vertex_t;
_Static_assert(sizeof(ocean_vertex_t)==32, "PVR vertex size");
typedef struct { float time,yaw,pitch; int swell,ripple,transmission,reflection,shadow,bloom; } ocean_settings_t;
static const sh4der_water_wave_t ocean_waves[] = {
    {0.28f,0.96f,0.130f,15.0f,4.84f,0.2f,0},
    {-0.60f,0.80f,0.075f,9.0f,3.75f,1.7f,0},
    {0.80f,0.60f,0.038f,5.4f,2.90f,4.0f,0},
    {-0.96f,0.28f,0.020f,3.2f,2.23f,2.2f,0},
    {0.28f,0.96f,0.0070f,0.78f,1.10f,0.8f,1},
    {-0.60f,0.80f,0.0038f,0.43f,0.82f,3.2f,1},
    {0.80f,0.60f,0.0017f,0.25f,0.62f,5.3f,1}
};
static const float ocean_fog_rgb[3] = {0.50f,0.29f,0.30f};
/* The sun is the live vector that the water, the shadow sweep, the transmission, the reflection and the sky texture all read. Its first values are
 * the original sunset (azimuth 9.806 deg right of the view axis, elevation 3.440 deg); SUN AZIMUTH / SUN ELEVATION replace it (ocean_sun_vector). */
static const float ocean_sun_original[3] = {0.170f,0.060f,0.9836158f};
static float ocean_sun[3] = {0.170f,0.060f,0.9836158f};
#define OCEAN_SUN_AZ_STEPS 11
#define OCEAN_SUN_EL_STEPS 10
#define OCEAN_SUN_AZ_ORIGINAL 6
#define OCEAN_SUN_EL_ORIGINAL 2
static const float ocean_sun_az_deg[OCEAN_SUN_AZ_STEPS] = {-40.0f,-30.0f,-20.0f,-10.0f,0.0f,5.0f,9.806f,15.0f,20.0f,30.0f,40.0f};
/* Lowest 1.5 deg: SH4DOW's sweep needs the sun above the horizon (sun y > 0). */
static const float ocean_sun_el_deg[OCEAN_SUN_EL_STEPS] = {1.5f,2.5f,3.440f,5.0f,8.0f,12.0f,18.0f,25.0f,35.0f,45.0f};
/* Water mesh steps (WATER TRIANGLES): 200 triangles apiece, 400 .. 22,800, then the original mesh (23,040 = 2 x 120 x 96). A step is a grid of columns x
 * rows (triangles 2 x c x r, vertices (c+1)*(r+1)): ocean_mesh_grid picks the grid nearest the target at an aspect near the original's 120:96; the arrays
 * below stay sized for the original. */
#define OCEAN_MESH_MIN_TRIS 400
#define OCEAN_MESH_TRIS_STEP 200
#define OCEAN_MESH_STEPS 114
#define OCEAN_MESH_ORIGINAL 113
#define OCEAN_MESH_STEP_FOR(tris) (((tris)-OCEAN_MESH_MIN_TRIS)/OCEAN_MESH_TRIS_STEP)
static unsigned ocean_mesh_w = OCEAN_X, ocean_mesh_h = OCEAN_Z;
static float ocean_eye = OCEAN_EYE, ocean_row_scale = 1.0f; /* the eye height; rows scale with it below the original height (the nearest row keeps its view angle) */
static unsigned ocean_mesh_target(unsigned step) { return step>=OCEAN_MESH_ORIGINAL ? 2u*OCEAN_X*OCEAN_Z : OCEAN_MESH_MIN_TRIS+OCEAN_MESH_TRIS_STEP*step; }
static void ocean_mesh_grid(unsigned step,unsigned *cols,unsigned *rows) {
    const unsigned n=ocean_mesh_target(step)/2;
    unsigned r,k,best=~0u,bc=OCEAN_X,br=OCEAN_Z;
    if(step>=OCEAN_MESH_ORIGINAL) { *cols=OCEAN_X; *rows=OCEAN_Z; return; }
    for(r=2;r<=OCEAN_Z;++r) for(k=0;k<2;++k) {
        const unsigned c=n/r+k;
        unsigned q,err,aspect,cost;
        if(c<2||c>OCEAN_X||c*100u<110u*r||c*100u>142u*r) continue; /* aspect 1.10 .. 1.42 (the original is 1.25) */
        q=c*r; err=q>n?q-n:n-q;
        aspect=c*100u/r>125u?c*100u/r-125u:125u-c*100u/r;
        cost=err*10000u/n+aspect*10u; /* units of 0.01 %: a 1 % miss of the target costs as much as 0.10 of aspect */
        if(cost<best) { best=cost; bc=c; br=r; }
    }
    *cols=bc; *rows=br;
}
static float ocean_col_step = 1.54f/OCEAN_X; /* the column spacing as a fraction of the row distance */
static ocean_vertex_t ocean_water[(OCEAN_X+1)*(OCEAN_Z+1)] __attribute__((aligned(32)));
static ocean_vertex_t ocean_sky[(OCEAN_SKY_X+1)*(OCEAN_SKY_Y+1)] __attribute__((aligned(32)));
static uint16_t ocean_environment[OCEAN_ENV*OCEAN_ENV] __attribute__((aligned(32)));
static float ocean_heights[OCEAN_SHADOW_X*OCEAN_SHADOW_Z] __attribute__((aligned(32)));
static float ocean_occlusion[OCEAN_SHADOW_X*OCEAN_SHADOW_Z] __attribute__((aligned(32)));
static float ocean_scratch[2*OCEAN_SHADOW_Z] __attribute__((aligned(32)));
static float ocean_rows[OCEAN_Z+1];
static float ocean_cols[OCEAN_X+1]; /* screen-side factor per column, x f gives metres */
/* Crest transmission (sh4der_sss): sun-coloured light, water absorption / m. */
static sh4der_sss_t ocean_sss;
#ifndef SH4DER_CORE_NO_SH4ZAM
/* One water row through shz_dce_project_vertices: 24 B source (side, dy, f + 3
 * unused words), 32 B records out. Row-sized: 7.4 KB, stays in cache. */
static float ocean_proj_in[(OCEAN_X+1)*6] __attribute__((aligned(32)));
static float ocean_proj_out[(OCEAN_X+1)*8] __attribute__((aligned(32)));
static uint32_t ocean_proj_words[OCEAN_X+1] __attribute__((aligned(32)));
static uint8_t ocean_proj_codes[OCEAN_X+1] __attribute__((aligned(32)));
/* Clip off (clip=0): every water vertex has depth >= ~3 m by construction
 * (rows start at 3.2 m, pitch clamped to -0.20..0.08). sz = 1/z'. */
static const shz_dce_view4_t ocean_view = {1.0f,-1.0f,1.0f,-1.0f,0.0f,240.0f,320.0f,0.01f,OCEAN_FOCAL,1.0f};
#endif
#ifdef _arch_dreamcast
static float ocean_cpu_us, ocean_frame_us, ocean_worst_us;
static unsigned ocean_over;
#endif

static uint32_t ocean_rgb(float r,float g,float b) {
    return 0xff000000u | ((uint32_t)sh4der_fma_(sh4der_sat_(r),255,0.5f)<<16) |
        ((uint32_t)sh4der_fma_(sh4der_sat_(g),255,0.5f)<<8) | (uint32_t)sh4der_fma_(sh4der_sat_(b),255,0.5f);
}
static float ocean_dot(const float a[3],const float b[3]) { return sh4der_dot3_(a,b); }
/* Unit or zero: SH4ZAM-DCE FSRRA normalize on SH4ZAM builds. */
static void ocean_norm3(float v[3]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    const shz_vec3_t n=shz_dce_vec3_normalize_safe(shz_vec3_init(v[0],v[1],v[2]));
    v[0]=n.x; v[1]=n.y; v[2]=n.z;
#else
    sh4der_norm3_(v);
#endif
}
/* Sun direction (toward the sun; x right, y up, z along the view axis) for an azimuth and an elevation step. The original pair gives the original
 * vector bit for bit; any other pair: (cos el sin az, sin el, cos el cos az), normalized. */
static void ocean_sun_vector(unsigned az,unsigned el,float out[3]) {
    float sa,ca,se,ce;
    if(az>=OCEAN_SUN_AZ_STEPS) az=OCEAN_SUN_AZ_ORIGINAL;
    if(el>=OCEAN_SUN_EL_STEPS) el=OCEAN_SUN_EL_ORIGINAL;
    if(az==OCEAN_SUN_AZ_ORIGINAL&&el==OCEAN_SUN_EL_ORIGINAL) { out[0]=ocean_sun_original[0]; out[1]=ocean_sun_original[1]; out[2]=ocean_sun_original[2]; return; }
    sh4der_sincos_(ocean_sun_az_deg[az]*(3.14159265f/180),&sa,&ca); /* hand: one constant scale */
    sh4der_sincos_(ocean_sun_el_deg[el]*(3.14159265f/180),&se,&ce);
    out[0]=ce*sa; out[1]=se; out[2]=ce*ca; /* hand: three products */
    ocean_norm3(out);
}
/* CPU preview mirror of the PVR custom fog table's continuous law. */
static float ocean_fog_amount(float depth) {
    return sh4der_sat_(sh4der_fma_(depth,1.0f/(OCEAN_FOG_END-OCEAN_FOG_START),-OCEAN_FOG_START/(OCEAN_FOG_END-OCEAN_FOG_START)));
}
static inline void ocean_fog_samples(float out[129]) {
    unsigned i;
    for(i=0;i<=128;++i) {
        /* PVR fog index: exponent i>>4, mantissa i&15 (power-of-two scale: exact) */
        float reciprocal=(float)(1u<<(i>>4))*sh4der_fma_((float)(i&15u),1.0f/16,1);
        out[i]=ocean_fog_amount(sh4der_div_(OCEAN_FOG_END,reciprocal));
    }
}
static void ocean_sky_rgb_sun(const float d[3], const float sun[3], float col[3]) {
    float altitude=sh4der_sat_(d[1]);
    float middle=sh4dow_smooth01(altitude*(1.0f/0.17f)); /* hand: one constant scale */
    float upper=sh4dow_smooth01(sh4der_fma_(altitude,1.0f/0.40f,-0.17f/0.40f));
    float sx=d[0]-sun[0],sy=d[1]-sun[1],sz=d[2]-sun[2];
    /* Chord distance remains stable under small FSRRA normalization errors;
     * a near-one dot threshold can lose the tiny solar disc entirely. */
    float distance2=sh4der_len3_(sx,sy,sz);
    float halo=sh4der_water_exp(-distance2*32.5f)*sh4dow_smooth01(altitude*40); /* hand: constant scales */
    float disc=1-sh4dow_smooth01(sh4der_fma_(distance2,1.0f/0.00018f,-0.00022f/0.00018f));
    const float twilight[3]={0.25f,0.16f,0.25f},zenith[3]={0.035f,0.065f,0.155f};
    unsigned c;
    for(c=0;c<3;++c)col[c]=sh4der_lerp_(sh4der_lerp_(ocean_fog_rgb[c],twilight[c],middle),zenith[c],upper);
    col[0]=sh4der_fma_(0.28f,halo,col[0]);col[1]=sh4der_fma_(0.19f,halo,col[1]);col[2]=sh4der_fma_(0.070f,halo,col[2]);
    col[0]=sh4der_lerp_(col[0],1.0f,disc);col[1]=sh4der_lerp_(col[1],0.91f,disc);col[2]=sh4der_lerp_(col[2],0.61f,disc);
}
static inline void ocean_sky_rgb(const float d[3], float col[3]) { ocean_sky_rgb_sun(d,ocean_sun,col); }
static void ocean_env_uv(const float r[3],float *u,float *v) {
    /* Upper-hemisphere paraboloid. Below-horizon rays use horizon colour. */
    float inv=sh4der_recip_pos_(1+(r[1]>0?r[1]:0));
    *u=sh4der_sat_(sh4der_fma_(0.499f*r[0],inv,0.5f)); *v=sh4der_sat_(sh4der_fma_(0.499f*r[2],inv,0.5f));
}
/* Row and column tables of the current water mesh. The row law is the original 96 row one evaluated at the row's fractional place y x 96 / rows
 * (exact at 96 rows); columns keep the original 120 column spacing law. The original mesh takes the original constants, so its tables are unchanged. */
static void ocean_mesh_tables(void) {
    unsigned x,y;
    const float colscale=ocean_mesh_w==OCEAN_X?2*0.77f/OCEAN_X:2*0.77f/(float)ocean_mesh_w; /* hand: one constant per mesh */
    ocean_col_step=ocean_mesh_w==OCEAN_X?1.54f/OCEAN_X:1.54f/(float)ocean_mesh_w;
    for(y=0;y<=ocean_mesh_h;++y) {
        /* Near rows follow projected depth; mid rows retain resolved swell
         * around the sun path; horizon rows can be widely spaced. */
        const float yy=ocean_mesh_h==OCEAN_Z?(float)y:(float)y*((float)OCEAN_Z/(float)ocean_mesh_h); /* hand: one constant per mesh */
        if(yy<=40) ocean_rows[y]=sh4der_div_(3.2f,sh4der_fma_(yy,-(1-3.2f/12)/40,1));
        else if(yy<=80) ocean_rows[y]=12*sh4der_water_exp(sh4der_fma_(yy,2.302585093f/40,-2.302585093f)); /* hand: constant scale */
        else ocean_rows[y]=120*sh4der_water_exp(sh4der_fma_(yy,3.14988295f/16,-80*3.14988295f/16)); /* hand: constant scale */
        if(ocean_row_scale!=1.0f) ocean_rows[y]*=ocean_row_scale; /* eye below 2.5 m: the water starts nearer, so the screen's bottom edge still meets water */
    }
    for(x=0;x<=ocean_mesh_w;++x) ocean_cols[x]=sh4der_fma_((float)x,colscale,-0.77f);
}
/* WATER TRIANGLES step: the mesh is rebuilt (tables here, every vertex on the next ocean_scene_update). */
static void ocean_mesh_set(unsigned step) {
    unsigned c,r;
    if(step>=OCEAN_MESH_STEPS) step=OCEAN_MESH_ORIGINAL;
    ocean_mesh_grid(step,&c,&r);
    ocean_mesh_w=c; ocean_mesh_h=r;
    ocean_mesh_tables();
}
/* Eye height in metres, clamped to OCEAN_EYE_MIN .. OCEAN_EYE_MAX. Camera pitch, the sky and the sun do not move: the horizon is at the pitch
 * angle for any height, and the sky rays come from yaw and pitch alone. Below the original 2.5 m the rows shrink with the eye (the water's near edge
 * would otherwise rise into the picture); at and above it they are unchanged, and 2.5 m is the original values exactly. */
static void ocean_eye_set(float eye) {
    ocean_eye=eye<OCEAN_EYE_MIN?OCEAN_EYE_MIN:eye>OCEAN_EYE_MAX?OCEAN_EYE_MAX:eye;
    ocean_row_scale=ocean_eye<OCEAN_EYE?ocean_eye*(1.0f/OCEAN_EYE):1.0f; /* hand: one constant scale */
    ocean_mesh_tables();
}
/* Environment texture rows y0..y1-1 for a sun. One call for all rows (ocean_scene_init) or a few at a time (the sun rows of the demo). */
static void ocean_env_rows(const float sun[3],unsigned y0,unsigned y1) {
    unsigned x,y;
    for(y=y0;y<y1;++y) for(x=0;x<OCEAN_ENV;++x) {
        float px=sh4der_fma_((float)x,2.0f/OCEAN_ENV,1.0f/OCEAN_ENV-1), pz=sh4der_fma_((float)y,2.0f/OCEAN_ENV,1.0f/OCEAN_ENV-1);
        float threshold=sh4der_warp_dither_threshold(x,y);
        /* hand: 2 inv, (1-r2) inv: scalar products of the paraboloid map */
        float r2=sh4der_len2_(px,pz), inv=sh4der_recip_pos_(1+r2), d[3]={2*px*inv,(1-r2)*inv,2*pz*inv}, rgb[3];
        ocean_sky_rgb_sun(d,sun,rgb);
        ocean_environment[y*OCEAN_ENV+x]=(uint16_t)(((unsigned)sh4der_fma_(sh4der_sat_(rgb[0]),31,threshold)<<11)|
            ((unsigned)sh4der_fma_(sh4der_sat_(rgb[1]),63,threshold)<<5)|(unsigned)sh4der_fma_(sh4der_sat_(rgb[2]),31,threshold));
    }
}
static void ocean_scene_init(void) {
    ocean_env_rows(ocean_sun,0,OCEAN_ENV);
    ocean_mesh_tables();
    { const float incident[3]={1.0f,0.58f,0.30f}, absorb[3]={0.50f,0.13f,0.07f};
      sh4der_sss_init(&ocean_sss,incident,absorb,0.25f,0.0f,0.0f); }
}
static void ocean_shadow_update(const ocean_settings_t *s) {
    unsigned x,z;
    if(!s->shadow) return;
    for(z=0;z<OCEAN_SHADOW_Z;++z) for(x=0;x<OCEAN_SHADOW_X;++x)
        ocean_heights[z*OCEAN_SHADOW_X+x]=sh4der_water_sample(ocean_waves,4,
            sh4der_fma_((float)x,OCEAN_CELL,OCEAN_ORIGIN_X),sh4der_fma_((float)z,OCEAN_CELL,OCEAN_ORIGIN_Z),s->time,0,(float)s->swell,0).height;
    sh4dow_heightfield_sun(ocean_heights,OCEAN_SHADOW_X,OCEAN_SHADOW_Z,OCEAN_CELL,
        ocean_sun,0.008f,0.035f,ocean_occlusion,ocean_scratch);
}
static float ocean_shadow_at(float x,float z) {
    float u=sh4der_fma_(x,1.0f/OCEAN_CELL,-OCEAN_ORIGIN_X/OCEAN_CELL), v=sh4der_fma_(z,1.0f/OCEAN_CELL,-OCEAN_ORIGIN_Z/OCEAN_CELL);
    unsigned ix,iz; float a,b,fade;
    if(u<1||u>=OCEAN_SHADOW_X-2||v<1||v>=OCEAN_SHADOW_Z-2) return 0;
    ix=(unsigned)u; iz=(unsigned)v; u-=ix; v-=iz; /* hand: integer split, u,v >= 1 here */
    a=sh4der_lerp_(ocean_occlusion[iz*OCEAN_SHADOW_X+ix],ocean_occlusion[iz*OCEAN_SHADOW_X+ix+1],u);
    b=sh4der_lerp_(ocean_occlusion[(iz+1)*OCEAN_SHADOW_X+ix],ocean_occlusion[(iz+1)*OCEAN_SHADOW_X+ix+1],u);
    fade=sh4dow_smooth01(sh4der_fma_(z,-0.12f,(OCEAN_ORIGIN_Z+(OCEAN_SHADOW_Z-2)*OCEAN_CELL)*0.12f));
    fade*=sh4dow_smooth01(sh4der_fma_(x,0.25f,-(OCEAN_ORIGIN_X+1)*0.25f))*sh4dow_smooth01(sh4der_fma_(x,-0.25f,(OCEAN_ORIGIN_X+(OCEAN_SHADOW_X-2)*OCEAN_CELL)*0.25f));
    return sh4der_lerp_(a,b,v)*fade;
}
static void ocean_scene_update(const ocean_settings_t *s) {
    float sy,cy,sp,cp; unsigned x,z;
    sh4der_sincos_(s->yaw,&sy,&cy); sh4der_sincos_(s->pitch,&sp,&cp);
    ocean_shadow_update(s);
    const unsigned mw=ocean_mesh_w,mh=ocean_mesh_h;
    for(z=0;z<=mh;++z) {
        const float f=ocean_rows[z], syf=sy*f, cyf=cy*f; /* hand: per-row scalar products */
        const float row_step=z<mh?ocean_rows[z+1]-f:f-ocean_rows[z-1];
        const float step0=f*ocean_col_step; /* hand: one scalar product */
        for(x=0;x<=mw;++x) {
            const float side=ocean_cols[x]*f; /* hand: one scalar product */
            /* Yaw: world (wx, wz) = Ry (side, f), one FMAC each */
            const float wx=sh4der_fma_(cy,side,syf), wz=sh4der_fma_(-sy,side,cyf);
            const float step=row_step>step0?row_step:step0;
            const sh4der_water_surface_t surf=sh4der_water_sample(ocean_waves,7,wx,wz,s->time,step,(float)s->swell,(float)s->ripple);
            const float dy=surf.height-ocean_eye;
            float n[3],view[3],r[3],rgb[3],trans[3];
            float ndv,fresnel,shadow,visibility,crest,light,texweight,base;
            ocean_vertex_t *v=&ocean_water[z*(mw+1)+x];
#ifndef SH4DER_CORE_NO_SH4ZAM
            /* Camera-yaw frame position; pitch + projection after the row, one DCE pass. */
            ocean_proj_in[x*6+0]=side; ocean_proj_in[x*6+1]=dy; ocean_proj_in[x*6+2]=f;
#else
            { /* hand: host mirror only (plain build has no SH4ZAM): pitch + divide */
              const float depth=f*cp+dy*sp, up=dy*cp-f*sp;
              v->x=320+OCEAN_FOCAL*side/depth; v->y=240-OCEAN_FOCAL*up/depth; v->z=1/depth; }
#endif
            sh4der_water_normal(surf,n);
            view[0]=-wx; view[1]=-dy; view[2]=-wz; ocean_norm3(view);
            ndv=sh4der_sat_(ocean_dot(n,view)); fresnel=sh4der_water_fresnel(ndv);
            sh4der_water_reflect(n,view,r); ocean_env_uv(r,&v->u,&v->v);
            shadow=s->shadow?ocean_shadow_at(wx,wz):0; visibility=1-shadow;
            crest=sh4der_sat_(sh4der_fma_(surf.height,3.4f,0.34f));
            /* Crest transmission: sh4der_sss, path 4.4 m (trough) to 0.4 m (crest top) */
            if(s->transmission) sh4der_water_crest_sss(&ocean_sss,view,ocean_sun,n,crest,0.4f,4.4f,1-fresnel,visibility,trans);
            else trans[0]=trans[1]=trans[2]=0;
            light=sh4der_sat_(ocean_dot(n,ocean_sun))*visibility; /* hand: one product */
            base=1-fresnel;
            rgb[0]=sh4der_fma_(base,sh4der_fma_(0.020f,light,0.014f),trans[0]);
            rgb[1]=sh4der_fma_(base,sh4der_fma_(0.032f,light,0.039f),trans[1]);
            rgb[2]=sh4der_fma_(base,sh4der_fma_(0.024f,light,0.051f),trans[2]);
            /* Occlusion removes direct sun from env only, keeps sky illumination.
             * CPU evaluates broad sun mask at vertex; glint texture supplies detail. */
            { const float sunmask=sh4der_water_exp(sh4der_fma_(sh4der_sat_(ocean_dot(r,ocean_sun)),260.0f,-260.0f));
              texweight=s->reflection?fresnel*sh4der_fma_(-shadow,sunmask,1.0f):0; }
            v->flags=0;
            v->argb=ocean_rgb(texweight,texweight,texweight); v->oargb=ocean_rgb(rgb[0],rgb[1],rgb[2])&0x00ffffffu;
        }
#ifndef SH4DER_CORE_NO_SH4ZAM
        /* XMTRX = pitch rotation (SH4ZAM), set per row right before use. Then
         * FTRV + FSRRA + FMAC screen point (SH4ZAM-DCE), 24 B in -> 32 B out. */
        shz_xmtrx_init_rotation_x(s->pitch);
        shz_dce_project_vertices(ocean_proj_out,ocean_proj_codes,ocean_proj_words,ocean_proj_in,24,mw+1,&ocean_view,0,0);
        for(x=0;x<=mw;++x) { /* copy only: record floats 5..7 = sx, sy, 1/z */
            ocean_vertex_t *v=&ocean_water[z*(mw+1)+x];
            v->x=ocean_proj_out[x*8+5]; v->y=ocean_proj_out[x*8+6]; v->z=ocean_proj_out[x*8+7];
        }
#endif
    }
#ifndef SH4DER_CORE_NO_SH4ZAM
    /* Sky ray = Ry(yaw) Rx(-pitch) (cx, cu, 1): SH4ZAM XMTRX, one FTRV a vertex. */
    shz_xmtrx_init_rotation_y(s->yaw); shz_xmtrx_rotate_x(-s->pitch);
#endif
    for(z=0;z<=OCEAN_SKY_Y;++z) for(x=0;x<=OCEAN_SKY_X;++x) {
        ocean_vertex_t *v=&ocean_sky[z*(OCEAN_SKY_X+1)+x];
        const float sx=(float)x*(640.0f/OCEAN_SKY_X), py=(float)z*(480.0f/OCEAN_SKY_Y); /* hand: exact grid scale */
        const float cx=sh4der_fma_(sx,1.0f/OCEAN_FOCAL,-320.0f/OCEAN_FOCAL), cu=sh4der_fma_(py,-1.0f/OCEAN_FOCAL,240.0f/OCEAN_FOCAL);
        float d[3];
#ifndef SH4DER_CORE_NO_SH4ZAM
        { const shz_vec3_t ray=shz_xmtrx_transform_vec3(shz_vec3_init(cx,cu,1.0f));
          d[0]=ray.x; d[1]=ray.y; d[2]=ray.z; }
#else
        d[0]=cy*cx+sy*(cp-sp*cu); d[1]=sp+cp*cu; d[2]=-sy*cx+cy*(cp-sp*cu); /* hand: host mirror only */
#endif
        ocean_norm3(d);
        ocean_env_uv(d,&v->u,&v->v);
        v->flags=0; v->x=sx;v->y=py;v->z=OCEAN_SKY_DEPTH;v->argb=0xffffffffu;v->oargb=0;
    }
}
#endif
