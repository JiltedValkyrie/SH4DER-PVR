/* Sunset ocean demo. Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 * Host preview rasterizes the SAME mesh/UV/vertex colours, without PVR bloom.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ocean_scene.h"
#include "sh4der_pvrx.h"
#ifdef _arch_dreamcast
#include <kos.h>
#include "sh4der_suite.h"
#include "sh4der_waitrun.h"
static sh4der_waitrun_t wr_run;
#include <dc/minifont.h>
#include "sh4der_frame.h"
#include "sh4der_glow.h"
#include "sh4der_guard.h"
KOS_INIT_FLAGS(INIT_DEFAULT);
_Static_assert(sizeof(ocean_vertex_t)==sizeof(pvr_vertex_t),"PVR ABI");
static pvr_poly_hdr_t env_header,water_header,hud_header;
static pvr_ptr_t env_texture,hud_texture;
#define HUD_H 128 /* five 18 px lines; a power of two for the texture */
static uint16_t hud_pixels[512*HUD_H] __attribute__((aligned(32)));
static uint16_t glow_grid[SH4DER_FRAME_GRID_X*SH4DER_FRAME_GRID_Y];
static sh4der_glow_t glow;
static int glow_ok;
#ifndef OCEAN_SERIAL_STATS
#define OCEAN_SERIAL_STATS 0
#endif
/* Sun rows: the environment texture (512 x 512) is rebuilt this many rows a frame after a sun change, so no frame stalls; the water keeps the old
 * sun until the texture is whole, then both switch together. 64 frames for 512 rows at the default 8. */
#ifndef OCEAN_EYE_RATE
#define OCEAN_EYE_RATE 2.0f /* metres a second at a full trigger */
#endif
#ifndef OCEAN_SKY_ROWS
#define OCEAN_SKY_ROWS 8
#endif
/* TA parameter buffer per frame: about 25.6k strip vertices in 6-triangle
 * blocks need about 0.96 MB; 1.0 MB left no margin. Real hardware stops the
 * list (no render, black screen) on overflow; an emulator does not. */
#ifndef OCEAN_VERTEX_BUF_KB
#define OCEAN_VERTEX_BUF_KB 1536
#endif
#ifndef OCEAN_OPB_OVERFLOW
#define OCEAN_OPB_OVERFLOW 6
#endif
/* Visible startup: each stage clears the displayed frame to one colour
 * (KOS vid_clear, no PVR render needed) and prints one serial line, once. */
#ifndef OCEAN_STAGE_HOLD_MS
#define OCEAN_STAGE_HOLD_MS 300
#endif
static uint64_t ocean_t0;
static unsigned long ocean_ta_kb,ocean_opb_kb;
static void ocean_stage(unsigned n,uint8_t r,uint8_t g,uint8_t b,const char *what) {
    vid_clear(r,g,b);
    printf("ocean: stage %u %s at %lu ms\n",n,what,(unsigned long)((timer_us_gettime64()-ocean_t0)/1000u));
    if(OCEAN_STAGE_HOLD_MS) thd_sleep(OCEAN_STAGE_HOLD_MS);
}
/* TA usage of the frame being registered: read while a list is still open,
 * before KOS re-arms the TA for the next frame. Register reads only. */
static void ocean_ta_meter(void) {
    uint32_t vb=(PVR_GET(PVR_TA_VERTBUF_POS)-PVR_GET(PVR_TA_VERTBUF_START))&0x00ffffffu;
    uint32_t ob=((PVR_GET(PVR_TA_OPB_POS)<<2)-PVR_GET(PVR_TA_OPB_INIT))&0x00ffffffu;
    if(vb/1024u>ocean_ta_kb) ocean_ta_kb=vb/1024u;
    if(ob/1024u>ocean_opb_kb) ocean_opb_kb=ob/1024u;
}
/* Rows: D-pad left / right picks one; A switches a toggle or steps a stepped row on (wraps; WATER TRIANGLES: ten steps, 2,000 triangles); D-pad up / down:
 * toggle on / off, stepped row up / down (WATER TRIANGLES: 200 triangles). LT lowers the eye, RT raises it (OCEAN_EYE_MIN .. OCEAN_EYE_MAX). */
enum { ROW_PRESET, ROW_SWELL, ROW_RIPPLE, ROW_TRANSMISSION, ROW_REFLECTION, ROW_SHADOW, ROW_BLOOM, ROW_MESH, ROW_SUN_AZ, ROW_SUN_EL, ROWS };
static const char *names[ROWS]={"PRESET","SWELL","RIPPLE","TRANSMISSION","REFLECTION","SH4DOW","BLOOM","WATER TRIANGLES","SUN AZIMUTH","SUN ELEVATION"};
/* A preset is the six toggles and the water mesh step. QUALITY is the demo's original look (the default). PERFORMANT: SH4DOW off (the heightfield
 * sweep is half the CPU work of a frame), the 2,000 triangle mesh; the other rows stay on because they cost almost nothing (host count:
 * ripples 0..3 %, reflection 0 %, transmission 17 % at the original mesh) and carry the look; BLOOM stays on, its cost is only measurable on a console. */
typedef struct { const char *name; int swell,ripple,transmission,reflection,shadow,bloom,mesh; } ocean_preset_t;
#define PRESETS 2
static const ocean_preset_t presets[PRESETS]={
    {"QUALITY",1,1,1,1,1,1,OCEAN_MESH_ORIGINAL},
    {"PERFORMANT",1,1,1,1,0,1,OCEAN_MESH_STEP_FOR(2000)}};
static int hud=0,paused=0,pick=ROW_PRESET;
static int mesh=OCEAN_MESH_ORIGINAL,sun_az=OCEAN_SUN_AZ_ORIGINAL,sun_el=OCEAN_SUN_EL_ORIGINAL;
static float sky_sun[3];           /* the sun the environment texture is being rebuilt for */
static unsigned sky_row;           /* next environment row to rebuild */
static int sky_pending,sky_commit,sky_upload,stats_reset;
static float ocean_render_us;
static unsigned mesh_vertices(void){return (ocean_mesh_w+1u)*(ocean_mesh_h+1u);}
static unsigned mesh_tris(void){return 2u*ocean_mesh_w*ocean_mesh_h;}
static int preset_now(const ocean_settings_t *s){
    int i;
    for(i=0;i<PRESETS;++i){const ocean_preset_t *p=presets+i;
        if(s->swell==p->swell&&s->ripple==p->ripple&&s->transmission==p->transmission&&s->reflection==p->reflection&&s->shadow==p->shadow&&s->bloom==p->bloom&&mesh==p->mesh)return i;}
    return PRESETS; /* CUSTOM */
}
/* Degrees to text in tenths, integer maths only (the demo prints no floats). */
static void deg_text(char *out,size_t n,float deg){
    int t=(int)(deg*10.0f+(deg<0?-0.5f:0.5f)); /* hand: rounding */
    snprintf(out,n,"%c%d.%d deg",t<0?'-':'+',(t<0?-t:t)/10,(t<0?-t:t)%10);
}
static void ocean_mesh_change(int m){
    if(m<0||m>=OCEAN_MESH_STEPS||m==mesh)return;
    mesh=m;ocean_mesh_set((unsigned)m);ocean_ta_kb=ocean_opb_kb=0;stats_reset=1;
    printf("ocean: water mesh %ux%u = %u triangles, %u vertices, %lu strip vertices a frame\n",(unsigned)ocean_mesh_w,(unsigned)ocean_mesh_h,mesh_tris(),mesh_vertices(),
        (unsigned long)(2u*(ocean_mesh_w+1u)*ocean_mesh_h));
}
static void ocean_sun_change(int az,int el){
    char a[24],e[24];
    if(az<0||az>=OCEAN_SUN_AZ_STEPS||el<0||el>=OCEAN_SUN_EL_STEPS||(az==sun_az&&el==sun_el))return;
    sun_az=az;sun_el=el;ocean_sun_vector((unsigned)az,(unsigned)el,sky_sun);
    sky_row=0;sky_pending=1;sky_commit=0;stats_reset=1; /* a restart while a rebuild runs: the water still has the old sun */
    deg_text(a,sizeof(a),ocean_sun_az_deg[az]);deg_text(e,sizeof(e),ocean_sun_el_deg[el]);
    printf("ocean: sun azimuth %s elevation %s, sky texture rebuild started\n",a,e);
}
static void ocean_preset_apply(ocean_settings_t *s,int i){
    const ocean_preset_t *p=presets+i;
    s->swell=p->swell;s->ripple=p->ripple;s->transmission=p->transmission;s->reflection=p->reflection;s->shadow=p->shadow;s->bloom=p->bloom;
    ocean_mesh_change(p->mesh);stats_reset=1;
    printf("ocean: preset %s\n",p->name);
}
/* Per frame, after the PVR waits: a few environment rows for the new sun. When the texture is whole the sun is committed at the start of the next
 * frame (so the water and the texture change in the same frame) and the texture is loaded to VRAM after that frame's wait. */
static void ocean_sky_step(void){
    unsigned end;
    if(!sky_pending)return;
    end=sky_row+OCEAN_SKY_ROWS;if(end>OCEAN_ENV)end=OCEAN_ENV;
    ocean_env_rows(sky_sun,sky_row,end);sky_row=end;
    if(sky_row>=OCEAN_ENV){sky_pending=0;sky_commit=1;}
}
static void ocean_headers(void) {
    pvr_poly_cxt_t c;
    env_texture=pvr_mem_malloc(sizeof(ocean_environment));
    hud_texture=pvr_mem_malloc(sizeof(hud_pixels));
    if(!env_texture||!hud_texture) { printf("ocean: texture allocation FAIL\n"); arch_exit(); }
    pvr_txr_load(ocean_environment,env_texture,sizeof(ocean_environment));
    pvr_poly_cxt_txr(&c,PVR_LIST_OP_POLY,PVR_TXRFMT_RGB565|PVR_TXRFMT_NONTWIDDLED,
        OCEAN_ENV,OCEAN_ENV,env_texture,PVR_FILTER_BILINEAR);
    c.gen.culling=PVR_CULLING_NONE; c.gen.fog_type=PVR_FOG_DISABLE;
    c.txr.env=PVR_TXRENV_MODULATE; c.txr.uv_clamp=PVR_UVCLAMP_UV;
    pvr_poly_compile(&env_header,&c);
    c.gen.fog_type=PVR_FOG_TABLE;
    c.gen.specular=1; /* Fixed-function offset RGB: water body + transmission. */
    pvr_poly_compile(&water_header,&c);
    pvr_poly_cxt_txr(&c,PVR_LIST_TR_POLY,PVR_TXRFMT_ARGB1555|PVR_TXRFMT_NONTWIDDLED,
        512,HUD_H,hud_texture,PVR_FILTER_NONE);
    c.gen.culling=PVR_CULLING_NONE; c.gen.fog_type=PVR_FOG_DISABLE;
    c.depth.comparison=PVR_DEPTHCMP_ALWAYS; c.depth.write=PVR_DEPTHWRITE_DISABLE;
    c.blend.src=PVR_BLEND_SRCALPHA;c.blend.dst=PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hud_header,&c);
}
/* KOS direct-render path: aligned 32-byte vertices, no hand store-queue code. */
static void ocean_submit(const ocean_vertex_t *grid,unsigned w,unsigned h,const pvr_poly_hdr_t *hdr) {
    unsigned x,y,k;
    uint32_t words[8];
    memcpy(words,hdr,sizeof(words));
    if(!sh4der_poly_hdr_ok(words)) return;
    pvr_prim(hdr,sizeof(*hdr));
    for(y=0;y<h;++y) for(x=0;x<=w;++x) for(k=0;k<2;++k) {
        const ocean_vertex_t *v=grid+(y+k)*(w+1)+x;
        pvr_vertex_t *out=pvr_dr_target();
        out->flags=(x==w&&k)?PVR_CMD_VERTEX_EOL:PVR_CMD_VERTEX;
        out->x=v->x;out->y=v->y;out->z=v->z;out->u=v->u;out->v=v->v;
        out->argb=v->argb;out->oargb=v->oargb;
        pvr_dr_commit(out);
    }
}
static void ocean_value(const ocean_settings_t *s,int row,char *out,size_t n) {
    const int toggles[]={0,s->swell,s->ripple,s->transmission,s->reflection,s->shadow,s->bloom};
    switch(row){
    case ROW_PRESET:{int p=preset_now(s);snprintf(out,n,"%s",p<PRESETS?presets[p].name:"CUSTOM");break;}
    case ROW_MESH:snprintf(out,n,"%u tris",mesh_tris());break;
    case ROW_SUN_AZ:deg_text(out,n,ocean_sun_az_deg[sun_az]);break;
    case ROW_SUN_EL:deg_text(out,n,ocean_sun_el_deg[sun_el]);break;
    default:snprintf(out,n,"%s",toggles[row]?"ON":"OFF");
    }
}
static void ocean_hud(const ocean_settings_t *s) {
    char text[96],value[32],a[24],e[24]; unsigned i;
    memset(hud_pixels,0,sizeof(hud_pixels)); minifont_set_color(255,255,255);
    ocean_value(s,pick,value,sizeof(value));
    snprintf(text,sizeof(text),"SUNSET  < %s %s >  %s",names[pick],value,paused?"PAUSED":"");
    minifont_draw_str(hud_pixels,512,text);
    snprintf(text,sizeof(text),"avg %lu.%lu ms  worst %lu.%lu ms  >33.3ms %u",
        (unsigned long)ocean_frame_us/1000,((unsigned long)ocean_frame_us/100)%10,
        (unsigned long)ocean_worst_us/1000,((unsigned long)ocean_worst_us/100)%10,ocean_over);
    minifont_draw_str(hud_pixels+512*18,512,text);
    snprintf(text,sizeof(text),"CPU %lu.%lu ms  RENDER %lu.%lu ms  VRAM %lu KB free",
        (unsigned long)ocean_cpu_us/1000,((unsigned long)ocean_cpu_us/100)%10,
        (unsigned long)ocean_render_us/1000,((unsigned long)ocean_render_us/100)%10,(unsigned long)pvr_mem_available()/1024);
    minifont_draw_str(hud_pixels+512*36,512,text);
    snprintf(text,sizeof(text),"WATER %u tris  %u verts (%ux%u)  TA %lu/%u KB",mesh_tris(),mesh_vertices(),(unsigned)ocean_mesh_w,(unsigned)ocean_mesh_h,
        ocean_ta_kb,(unsigned)OCEAN_VERTEX_BUF_KB);
    minifont_draw_str(hud_pixels+512*54,512,text);
    deg_text(a,sizeof(a),ocean_sun_az_deg[sun_az]);deg_text(e,sizeof(e),ocean_sun_el_deg[sun_el]);
    if(sky_pending)snprintf(value,sizeof(value),"REBUILD %u%%",sky_row*100u/OCEAN_ENV);else snprintf(value,sizeof(value),"READY");
    { const int t=(int)(ocean_eye*10.0f+0.5f); /* hand: rounding, tenths of a metre */
      snprintf(text,sizeof(text),"SUN %s / %s  SKY %s  EYE %d.%d m",a,e,value,t/10,t%10); }
    minifont_draw_str(hud_pixels+512*72,512,text);
    for(i=0;i<512*HUD_H;++i) hud_pixels[i]=hud_pixels[i]?0xffff:0;
    pvr_txr_load(hud_pixels,hud_texture,sizeof(hud_pixels));
}
static void ocean_hud_draw(void) {
    unsigned i; pvr_prim(&hud_header,sizeof(hud_header));
    for(i=0;i<4;++i) {
        pvr_vertex_t *v=pvr_dr_target();
        v->flags=i==3?PVR_CMD_VERTEX_EOL:PVR_CMD_VERTEX;
        v->x=20+(i&1?512:0);v->y=18+(i&2?HUD_H:0);v->z=1000;
        v->u=i&1?1:0;v->v=i&2?1:0;v->argb=0xffb0b0b0u;v->oargb=0;pvr_dr_commit(v);
    }
}
/* One change to the picked row: dir +1 / -1 (D-pad up / down), or a = 1 (A: a toggle flips, a stepped row goes one up and wraps). */
static int step_index(int v,int n,int dir,int a){ if(a) return (v+1)%n; v+=dir; return v<0?0:v>=n?n-1:v; }
static void ocean_step(ocean_settings_t *s,int dir,int a) {
    int *toggles[]={NULL,&s->swell,&s->ripple,&s->transmission,&s->reflection,&s->shadow,&s->bloom};
    switch(pick){
    case ROW_PRESET:{int now=preset_now(s);ocean_preset_apply(s,now>=PRESETS?0:step_index(now,PRESETS,dir,a));break;}
    case ROW_MESH:ocean_mesh_change(a?(mesh+10)%OCEAN_MESH_STEPS:step_index(mesh,OCEAN_MESH_STEPS,dir,a));break;
    case ROW_SUN_AZ:ocean_sun_change(step_index(sun_az,OCEAN_SUN_AZ_STEPS,dir,a),sun_el);break;
    case ROW_SUN_EL:ocean_sun_change(sun_az,step_index(sun_el,OCEAN_SUN_EL_STEPS,dir,a));break;
    default:{int *t=toggles[pick],now=*t;*t=a?!now:dir>0;if(*t!=now)stats_reset=1;}
    }
}
static void ocean_input(ocean_settings_t *s,float dt) {
    static uint32_t last=0;
    maple_device_t *dev=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
    cont_state_t *st=dev?(cont_state_t*)maple_dev_status(dev):NULL;
    if (st && suite_present() && (st->buttons & SUITE_KEYS_ALL) == SUITE_KEYS_ALL) { sh4der_pvrx_wait_hook(0); suite_exit(SUITE_KEYS); }   /* the test suite disc: A+B+X+Y back to the menu */
    uint32_t down;
    if(!st)return;
    down=st->buttons&~last;last=st->buttons;
    if(down&CONT_DPAD_LEFT)pick=(pick+ROWS-1)%ROWS;
    if(down&CONT_DPAD_RIGHT)pick=(pick+1)%ROWS;
    if(down&CONT_A)ocean_step(s,0,1);
    if(down&CONT_DPAD_UP)ocean_step(s,1,0);
    if(down&CONT_DPAD_DOWN)ocean_step(s,-1,0);
    if(down&CONT_X)paused^=1;
    if(down&CONT_Y)hud^=1;
    if(down&CONT_START){s->yaw=0;s->pitch=-0.045f;}
    { const float lift=((float)st->rtrig-(float)st->ltrig)*(1.0f/255); /* RT raises, LT lowers: 0 .. 1 of OCEAN_EYE_RATE metres a second */
      if(shz_fabsf(lift)>0.03f)ocean_eye_set(shz_fmaf(lift,dt*OCEAN_EYE_RATE,ocean_eye)); }
    if(shz_fabsf(st->joyx)>12)s->yaw=shz_clampf(shz_fmaf((float)st->joyx,dt*(1.0f/160.0f),s->yaw),-0.65f,0.65f); /* hand: dt x constant */
    if(shz_fabsf(st->joyy)>12)s->pitch=shz_clampf(shz_fmaf((float)st->joyy,dt*(-1.0f/320.0f),s->pitch),-0.20f,0.08f); /* hand: dt x constant */
}
int main(void) {
    suite_init();   /* on the test suite disc: the menu's binary into RAM, before anything else */
    if (suite_present()) sh4der_waitrun_init(&wr_run, SH4DER_WAITRUN_SECONDS);   /* from the suite's menu: a timed run that ends on a results page */
    ocean_settings_t s={0,0,-0.045f,1,1,1,1,1,1};
    if (suite_present()) ocean_preset_apply(&s,1);   /* the timed run uses PERFORMANT (about 2,000 water triangles): the QUALITY look is 8 frames a second and hides the wait */
    pvr_init_params_t init=pvr_default_params;
    uint64_t previous,report,cpu_total=0,frame_total=0,render_total=0;unsigned frames=0,over=0;uint64_t worst=0;
    pvr_stats_t stats;
    unsigned long total_frames=0;unsigned stuck=0;int stuck_shown=0,ta_reported=0,rendered=0;
    ocean_t0=timer_us_gettime64();
    /* Stage colours (displayed frame, no PVR render): 1 red main, 2 orange PVR init,
     * 3 yellow fog table, 4 green sky texture built, 5 cyan VRAM upload + headers,
     * 6 blue first wave/shadow update, 7 magenta first frame submit. Ocean replaces
     * it when the PVR renders. White later = PVR never finished a frame. */
    ocean_stage(1,160,0,0,"main");
    init.vertex_buf_size=OCEAN_VERTEX_BUF_KB*1024;init.opb_overflow_count=OCEAN_OPB_OVERFLOW;init.autosort_disabled=1;
    if(pvr_init(&init)<0){printf("ocean: pvr_init FAIL\n");return 1;}

#ifndef WAIT_SLEEP
    sh4der_pvrx_wait_hook(1);   /* polled render-done wait (PVR-117): every scene below ends through sh4der_pvrx_scene_finish; -DWAIT_SLEEP keeps KallistiOS's sleeping wait for an A/B */
#endif
    pvr_set_bg_color(0.1f,0.1f,0.2f);
    ocean_stage(2,224,96,0,"pvr_init");
    { float fog[129];
      ocean_fog_samples(fog);
      pvr_fog_table_color(1,ocean_fog_rgb[0],ocean_fog_rgb[1],ocean_fog_rgb[2]);
      pvr_fog_far_depth(OCEAN_FOG_END);pvr_fog_table_custom(fog); }
    ocean_stage(3,200,200,0,"fog table");
    ocean_scene_init();
    ocean_stage(4,0,160,0,"sky texture built");
    ocean_headers();glow_ok=sh4der_glow_init(&glow);glow.keep=0;
    ocean_stage(5,0,160,160,"VRAM upload + headers");
    printf("ocean: grid %ux%u, environment 512 KB, shadow field %lu KB, AICA effect 0 KB, VRAM free %lu KB, TA vertex buffer %u KB x2, OPB overflow %u\n",
        (unsigned)ocean_mesh_w,(unsigned)ocean_mesh_h,(unsigned long)(sizeof(ocean_heights)+sizeof(ocean_occlusion)+sizeof(ocean_scratch))/1024,
        (unsigned long)pvr_mem_available()/1024,(unsigned)OCEAN_VERTEX_BUF_KB,(unsigned)OCEAN_OPB_OVERFLOW);
    { uint64_t t=timer_us_gettime64(); ocean_scene_update(&s);
      printf("ocean: first wave/shadow update %lu us\n",(unsigned long)(timer_us_gettime64()-t)); }
    ocean_stage(6,0,0,200,"first update");
    ocean_stage(7,200,0,200,"first submit");
    previous=report=timer_us_gettime64();
    for(;;){
        if (suite_present() && sh4der_waitrun_frame(&wr_run, timer_us_gettime64())) sh4der_waitrun_show(&wr_run, "OCEAN");
        uint64_t now=timer_us_gettime64(),elapsed=now-previous,cpu_start,work;float dt;
        int ready;
        previous=now;dt=shz_fminf((float)elapsed*1e-6f,0.1f); /* hand: us to s, one product */
        ocean_input(&s,dt);
        if(stats_reset){stats_reset=0;report=now;cpu_total=frame_total=render_total=0;frames=0;worst=0;over=0;} /* a change: a clean two-second window */
        if(sky_commit){ocean_sun[0]=sky_sun[0];ocean_sun[1]=sky_sun[1];ocean_sun[2]=sky_sun[2];sky_commit=0;sky_upload=1;} /* water + shadows + sky switch together */
        if(!paused)s.time+=dt;
        /* One elapsed-time clock drives all waves and shadows. */
        cpu_start=timer_us_gettime64();ocean_scene_update(&s);
        work=timer_us_gettime64()-cpu_start;
        ready=sh4der_pvrx_wait_idle(); /* texture update only after use: every frame, so the polled wait (PVR-117) */
        if(pvr_get_stats(&stats)==0)render_total+=stats.rnd_last_time/1000u; /* ns of the frame just rendered, as us */
        if(sky_upload){pvr_txr_load(ocean_environment,env_texture,sizeof(ocean_environment));sky_upload=0;}
        /* KOS times each wait out at 100 ms. Twenty in a row = the PVR never
         * finished a frame (TA overflow / bad list): paint white once, log once. */
        if(ready<0){
            if(++stuck>=20&&!stuck_shown){
                stuck_shown=1;vid_clear(255,255,255);
                printf("ocean: PVR stuck after %lu frames: TA %lu KB, OPB overflow %lu KB\n",total_frames,ocean_ta_kb,ocean_opb_kb);
            }
        } else {
            if(!rendered&&total_frames>=1){ /* once: first frame through the PVR */
                rendered=1;
                printf("ocean: frame 1 rendered by %lu ms\n",(unsigned long)((timer_us_gettime64()-ocean_t0)/1000u));
            }
            stuck=0;
        }
        cpu_start=timer_us_gettime64();
        if(s.bloom&&glow_ok&&frames>0){
            unsigned x,y;
            sh4der_frame_read(glow_grid,NULL,640,480);
            /* Keep only sun path. Exclude broad bright sky and prior halo:
             * threshold high; weak 0.10 gain limits feedback from prior halo. */
            for(y=0;y<SH4DER_FRAME_GRID_Y;++y)for(x=0;x<SH4DER_FRAME_GRID_X;++x){
                if(y*8u+4u<100u)glow_grid[y*SH4DER_FRAME_GRID_X+x]=0; /* cell centre row < 100 px: integer test, no float */
            }
            sh4der_glow_upload(&glow,glow_grid);
        }
        ocean_sky_step();
        if(hud)ocean_hud(&s);
        pvr_scene_begin();pvr_list_begin(PVR_LIST_OP_POLY);
        ocean_submit(ocean_sky,OCEAN_SKY_X,OCEAN_SKY_Y,&env_header);
        ocean_submit(ocean_water,ocean_mesh_w,ocean_mesh_h,&water_header);
        pvr_list_finish();pvr_list_begin(PVR_LIST_TR_POLY);
        if(s.bloom&&glow_ok&&frames>0)sh4der_glow_draw_halo(&glow,0,0,640,480,10,0.10f,1.8f,0);
        if(hud)ocean_hud_draw();
        ocean_ta_meter(); /* TR list still open: TA not yet re-armed */
        pvr_list_finish();sh4der_pvrx_scene_finish();
        ++total_frames;
        /* Two serial lines total: after the first frame and after ~10 s. */
        if((total_frames==1&&ta_reported==0)||(total_frames>=300&&ta_reported==1)){
            ++ta_reported;
            printf("ocean: frame %lu at %lu ms: TA vertex %lu of %u KB, OPB overflow %lu KB, waits timed out %u\n",
                total_frames,(unsigned long)((timer_us_gettime64()-ocean_t0)/1000u),ocean_ta_kb,(unsigned)OCEAN_VERTEX_BUF_KB,ocean_opb_kb,stuck);
        }
        cpu_total+=work+timer_us_gettime64()-cpu_start;
        if(frames>0){frame_total+=elapsed;if(elapsed>worst)worst=elapsed;over+=elapsed>33334;}
        ++frames;
        if(now-report>=2000000&&frames>1){

            ocean_cpu_us=(float)cpu_total/frames;ocean_frame_us=(float)frame_total/(frames-1);
            ocean_worst_us=(float)worst;ocean_over=over;ocean_render_us=(float)render_total/frames;
#if OCEAN_SERIAL_STATS
            printf("ocean: frame_avg=%lu us worst=%lu us over_33ms=%u cpu=%lu us render=%lu us vram_free=%lu KB shadow=%d ripple=%d transmission=%d\n",
                (unsigned long)ocean_frame_us,(unsigned long)worst,over,(unsigned long)ocean_cpu_us,
                (unsigned long)ocean_render_us,(unsigned long)pvr_mem_available()/1024,s.shadow,s.ripple,s.transmission);
#endif
            report=now;cpu_total=frame_total=render_total=0;frames=0;worst=0;over=0;
        }
    }
}
#else
/* Small offline rasterizer for inspecting geometry/colour/UV before console boot.
 * Host time is NOT a Dreamcast performance measurement. */
static float pixels[480][640][3],depths[480][640];
static void ocean_texel(float u,float v,float rgb[3]){
    float fx=sh4der_sat_(u)*OCEAN_ENV-0.5f,fy=sh4der_sat_(v)*OCEAN_ENV-0.5f;
    int ix=(int)floorf(fx),iy=(int)floorf(fy),x,y;unsigned c;
    float a=fx-ix,b=fy-iy;
    for(c=0;c<3;++c)rgb[c]=0;
    for(y=0;y<2;++y)for(x=0;x<2;++x){
        int tx=ix+x,ty=iy+y;uint16_t p;float weight=(x?a:1-a)*(y?b:1-b);
        if(tx<0)tx=0;
        if(tx>=OCEAN_ENV)tx=OCEAN_ENV-1;
        if(ty<0)ty=0;
        if(ty>=OCEAN_ENV)ty=OCEAN_ENV-1;
        p=ocean_environment[ty*OCEAN_ENV+tx];
        rgb[0]+=weight*(p>>11)*(1.0f/31);rgb[1]+=weight*((p>>5)&63)*(1.0f/63);rgb[2]+=weight*(p&31)*(1.0f/31);
    }
}
static float edge(const ocean_vertex_t *a,const ocean_vertex_t *b,float x,float y){return (x-a->x)*(b->y-a->y)-(y-a->y)*(b->x-a->x);}
static void triangle(const ocean_vertex_t *a,const ocean_vertex_t *b,const ocean_vertex_t *c,int fogged){
    float area=edge(a,b,c->x,c->y),inv;int x,y,minx,maxx,miny,maxy;
    if(fabsf(area)<1e-7f)return;
    inv=1/area;
    minx=(int)fmaxf(0,floorf(fminf(a->x,fminf(b->x,c->x))));maxx=(int)fminf(639,ceilf(fmaxf(a->x,fmaxf(b->x,c->x))));
    miny=(int)fmaxf(0,floorf(fminf(a->y,fminf(b->y,c->y))));maxy=(int)fminf(479,ceilf(fmaxf(a->y,fmaxf(b->y,c->y))));
    for(y=miny;y<=maxy;++y)for(x=minx;x<=maxx;++x){
        float wa=edge(b,c,x+0.5f,y+0.5f)*inv,wb=edge(c,a,x+0.5f,y+0.5f)*inv,wc=1-wa-wb,z,rgb[3];unsigned ch;
        if(wa<0||wb<0||wc<0)continue;
        z=wa*a->z+wb*b->z+wc*c->z;
        if(z<depths[y][x])continue;
        depths[y][x]=z;
        ocean_texel((wa*a->u*a->z+wb*b->u*b->z+wc*c->u*c->z)/z,(wa*a->v*a->z+wb*b->v*b->z+wc*c->v*c->z)/z,rgb);
        for(ch=0;ch<3;++ch){unsigned shift=16-8*ch;
            float gain=(wa*((a->argb>>shift)&255)+wb*((b->argb>>shift)&255)+wc*((c->argb>>shift)&255))*(1.0f/255);
            float add=(wa*((a->oargb>>shift)&255)+wb*((b->oargb>>shift)&255)+wc*((c->oargb>>shift)&255))*(1.0f/255);
            pixels[y][x][ch]=sh4der_lerp_(sh4der_sat_(rgb[ch]*gain+add),ocean_fog_rgb[ch],fogged?ocean_fog_amount(1/z):0);
        }
    }
}
static void raster_grid(const ocean_vertex_t *grid,unsigned w,unsigned h,int fogged){unsigned x,y;
    for(y=0;y<h;++y)for(x=0;x<w;++x){const ocean_vertex_t *a=grid+y*(w+1)+x,*b=a+1,*c=a+w+1,*d=c+1;triangle(a,c,b,fogged);triangle(b,c,d,fogged);}}
int main(int argc,char **argv){
    ocean_settings_t s={6.0f,0,-0.045f,1,1,1,1,1,0};FILE *f;unsigned x,y;unsigned char rgb[3];
    int i,mesh=OCEAN_MESH_ORIGINAL,az=OCEAN_SUN_AZ_ORIGINAL,el=OCEAN_SUN_EL_ORIGINAL;float eye=OCEAN_EYE;
    if(argc<2){fprintf(stderr,"usage: ocean-preview output.ppm [seconds] [no-ripple|no-shadow|no-transmission|flat|performant|tris=400..22800 (multiple of 200)|mesh=0..113|eye=0.8..8|az=0..10|el=0..9]...\n");return 2;}
    if(argc>2)s.time=(float)atof(argv[2]);
    for(i=3;i<argc;++i){
        if(!strcmp(argv[i],"no-ripple"))s.ripple=0;else if(!strcmp(argv[i],"no-shadow"))s.shadow=0;else if(!strcmp(argv[i],"no-transmission"))s.transmission=0;else if(!strcmp(argv[i],"flat"))s.swell=s.ripple=0;
        else if(!strcmp(argv[i],"performant")){s.shadow=0;mesh=OCEAN_MESH_STEP_FOR(2000);}
        else if(!strncmp(argv[i],"mesh=",5))mesh=atoi(argv[i]+5);else if(!strncmp(argv[i],"tris=",5))mesh=OCEAN_MESH_STEP_FOR(atoi(argv[i]+5));else if(!strncmp(argv[i],"eye=",4))eye=(float)atof(argv[i]+4);else if(!strncmp(argv[i],"az=",3))az=atoi(argv[i]+3);else if(!strncmp(argv[i],"el=",3))el=atoi(argv[i]+3);
    }
    /* Model KOS background plane, so a sky behind it disappears here too. */
    for(y=0;y<480;++y)for(x=0;x<640;++x){depths[y][x]=OCEAN_BACKGROUND_DEPTH;pixels[y][x][0]=0.1f;pixels[y][x][1]=0.1f;pixels[y][x][2]=0.2f;}
    ocean_sun_vector((unsigned)az,(unsigned)el,ocean_sun);ocean_scene_init();ocean_mesh_set((unsigned)mesh);ocean_eye_set(eye);ocean_scene_update(&s);
    raster_grid(ocean_sky,OCEAN_SKY_X,OCEAN_SKY_Y,0);raster_grid(ocean_water,ocean_mesh_w,ocean_mesh_h,1);
    f=fopen(argv[1],"wb");if(!f)return 1;fprintf(f,"P6\n640 480\n255\n");
    for(y=0;y<480;++y)for(x=0;x<640;++x){unsigned c;for(c=0;c<3;++c)rgb[c]=(unsigned char)(pixels[y][x][c]*255+0.5f);fwrite(rgb,1,3,f);}
    fclose(f);printf("preview: %s, water %u triangles %u vertices, eye %.1f m, sun %.1f / %.1f deg; host raster, no PVR bloom or hardware timing\n",argv[1],2*ocean_mesh_w*ocean_mesh_h,(ocean_mesh_w+1)*(ocean_mesh_h+1),(double)ocean_eye,(double)ocean_sun_az_deg[az],(double)ocean_sun_el_deg[el]);return 0;
}
#endif
