/* Sunset ocean demo. Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 * Host preview rasterizes the SAME mesh/UV/vertex colours, without PVR bloom.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ocean_scene.h"
#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/minifont.h>
#include "sh4der_frame.h"
#include "sh4der_glow.h"
#include "sh4der_guard.h"
KOS_INIT_FLAGS(INIT_DEFAULT);
_Static_assert(sizeof(ocean_vertex_t)==sizeof(pvr_vertex_t),"PVR ABI");
static pvr_poly_hdr_t env_header,water_header,hud_header;
static pvr_ptr_t env_texture,hud_texture;
static uint16_t hud_pixels[512*64] __attribute__((aligned(32)));
static uint16_t glow_grid[SH4DER_FRAME_GRID_X*SH4DER_FRAME_GRID_Y];
static sh4der_glow_t glow;
static int glow_ok;
#ifndef OCEAN_SERIAL_STATS
#define OCEAN_SERIAL_STATS 0
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
static int hud=0,paused=0,pick=2;
static const char *names[]={"SWELL","RIPPLE","TRANSMISSION","REFLECTION","SH4DOW","BLOOM"};
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
        512,64,hud_texture,PVR_FILTER_NONE);
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
static void ocean_hud(const ocean_settings_t *s) {
    char text[64]; unsigned i; const int toggles[]={s->swell,s->ripple,s->transmission,s->reflection,s->shadow,s->bloom};
    memset(hud_pixels,0,sizeof(hud_pixels)); minifont_set_color(255,255,255);
    snprintf(text,sizeof(text),"SUNSET  < %s %s >  %s",names[pick],toggles[pick]?"ON":"OFF",paused?"PAUSED":"");
    minifont_draw_str(hud_pixels,512,text);
    snprintf(text,sizeof(text),"avg %lu.%lu ms  worst %lu.%lu ms  >33.3ms %u",
        (unsigned long)ocean_frame_us/1000,((unsigned long)ocean_frame_us/100)%10,
        (unsigned long)ocean_worst_us/1000,((unsigned long)ocean_worst_us/100)%10,ocean_over);
    minifont_draw_str(hud_pixels+512*18,512,text);
    snprintf(text,sizeof(text),"CPU %lu.%lu ms  VRAM %lu KB free  TA %lu/%u KB",
        (unsigned long)ocean_cpu_us/1000,((unsigned long)ocean_cpu_us/100)%10,(unsigned long)pvr_mem_available()/1024,
        ocean_ta_kb,(unsigned)OCEAN_VERTEX_BUF_KB);
    minifont_draw_str(hud_pixels+512*36,512,text);
    for(i=0;i<512*64;++i) hud_pixels[i]=hud_pixels[i]?0xffff:0;
    pvr_txr_load(hud_pixels,hud_texture,sizeof(hud_pixels));
}
static void ocean_hud_draw(void) {
    unsigned i; pvr_prim(&hud_header,sizeof(hud_header));
    for(i=0;i<4;++i) {
        pvr_vertex_t *v=pvr_dr_target();
        v->flags=i==3?PVR_CMD_VERTEX_EOL:PVR_CMD_VERTEX;
        v->x=20+(i&1?512:0);v->y=18+(i&2?64:0);v->z=1000;
        v->u=i&1?1:0;v->v=i&2?1:0;v->argb=0xffb0b0b0u;v->oargb=0;pvr_dr_commit(v);
    }
}
static void ocean_input(ocean_settings_t *s,float dt) {
    static uint32_t last=0;
    maple_device_t *dev=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
    cont_state_t *st=dev?(cont_state_t*)maple_dev_status(dev):NULL;
    int *toggles[]={&s->swell,&s->ripple,&s->transmission,&s->reflection,&s->shadow,&s->bloom};
    uint32_t down;
    if(!st)return;
    down=st->buttons&~last;last=st->buttons;
    if(down&CONT_DPAD_LEFT)pick=(pick+5)%6;
    if(down&CONT_DPAD_RIGHT)pick=(pick+1)%6;
    if(down&CONT_A)*toggles[pick]^=1;
    if(down&CONT_X)paused^=1;
    if(down&CONT_Y)hud^=1;
    if(down&CONT_START){s->yaw=0;s->pitch=-0.045f;}
    if(shz_fabsf(st->joyx)>12)s->yaw=shz_clampf(shz_fmaf((float)st->joyx,dt*(1.0f/160.0f),s->yaw),-0.65f,0.65f); /* hand: dt x constant */
    if(shz_fabsf(st->joyy)>12)s->pitch=shz_clampf(shz_fmaf((float)st->joyy,dt*(-1.0f/320.0f),s->pitch),-0.20f,0.08f); /* hand: dt x constant */
}
int main(void) {
    ocean_settings_t s={0,0,-0.045f,1,1,1,1,1,1};
    pvr_init_params_t init=pvr_default_params;
    uint64_t previous,report,cpu_total=0,frame_total=0;unsigned frames=0,over=0;uint64_t worst=0;
    unsigned long total_frames=0;unsigned stuck=0;int stuck_shown=0,ta_reported=0,rendered=0;
    ocean_t0=timer_us_gettime64();
    /* Stage colours (displayed frame, no PVR render): 1 red main, 2 orange PVR init,
     * 3 yellow fog table, 4 green sky texture built, 5 cyan VRAM upload + headers,
     * 6 blue first wave/shadow update, 7 magenta first frame submit. Ocean replaces
     * it when the PVR renders. White later = PVR never finished a frame. */
    ocean_stage(1,160,0,0,"main");
    init.vertex_buf_size=OCEAN_VERTEX_BUF_KB*1024;init.opb_overflow_count=OCEAN_OPB_OVERFLOW;init.autosort_disabled=1;
    if(pvr_init(&init)<0){printf("ocean: pvr_init FAIL\n");return 1;}
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
        OCEAN_X,OCEAN_Z,(unsigned long)(sizeof(ocean_heights)+sizeof(ocean_occlusion)+sizeof(ocean_scratch))/1024,
        (unsigned long)pvr_mem_available()/1024,(unsigned)OCEAN_VERTEX_BUF_KB,(unsigned)OCEAN_OPB_OVERFLOW);
    { uint64_t t=timer_us_gettime64(); ocean_scene_update(&s);
      printf("ocean: first wave/shadow update %lu us\n",(unsigned long)(timer_us_gettime64()-t)); }
    ocean_stage(6,0,0,200,"first update");
    ocean_stage(7,200,0,200,"first submit");
    previous=report=timer_us_gettime64();
    for(;;){
        uint64_t now=timer_us_gettime64(),elapsed=now-previous,cpu_start,work;float dt;
        int ready;
        previous=now;dt=shz_fminf((float)elapsed*1e-6f,0.1f); /* hand: us to s, one product */
        ocean_input(&s,dt);
        if(!paused)s.time+=dt;
        /* One elapsed-time clock drives all waves and shadows. */
        cpu_start=timer_us_gettime64();ocean_scene_update(&s);
        work=timer_us_gettime64()-cpu_start;
        ready=pvr_wait_ready();ready|=pvr_wait_render_done(); /* texture update only after use */
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
        if(hud)ocean_hud(&s);
        pvr_scene_begin();pvr_list_begin(PVR_LIST_OP_POLY);
        ocean_submit(ocean_sky,OCEAN_SKY_X,OCEAN_SKY_Y,&env_header);
        ocean_submit(ocean_water,OCEAN_X,OCEAN_Z,&water_header);
        pvr_list_finish();pvr_list_begin(PVR_LIST_TR_POLY);
        if(s.bloom&&glow_ok&&frames>0)sh4der_glow_draw_halo(&glow,0,0,640,480,10,0.10f,1.8f,0);
        if(hud)ocean_hud_draw();
        ocean_ta_meter(); /* TR list still open: TA not yet re-armed */
        pvr_list_finish();pvr_scene_finish();
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
            ocean_worst_us=(float)worst;ocean_over=over;
#if OCEAN_SERIAL_STATS
            pvr_stats_t stats;pvr_get_stats(&stats);
            printf("ocean: frame_avg=%lu us worst=%lu us over_33ms=%u cpu=%lu us render=%lu us vram_free=%lu KB shadow=%d ripple=%d transmission=%d\n",
                (unsigned long)ocean_frame_us,(unsigned long)worst,over,(unsigned long)ocean_cpu_us,
                (unsigned long)(stats.rnd_last_time/1000u),(unsigned long)pvr_mem_available()/1024,s.shadow,s.ripple,s.transmission);
#endif
            report=now;cpu_total=frame_total=0;frames=0;worst=0;over=0;
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
    if(argc<2){fprintf(stderr,"usage: ocean-preview output.ppm [seconds] [no-ripple|no-shadow|no-transmission|flat]\n");return 2;}
    if(argc>2)s.time=(float)atof(argv[2]);
    if(argc>3){if(!strcmp(argv[3],"no-ripple"))s.ripple=0;else if(!strcmp(argv[3],"no-shadow"))s.shadow=0;else if(!strcmp(argv[3],"no-transmission"))s.transmission=0;else if(!strcmp(argv[3],"flat"))s.swell=s.ripple=0;}
    /* Model KOS background plane, so a sky behind it disappears here too. */
    for(y=0;y<480;++y)for(x=0;x<640;++x){depths[y][x]=OCEAN_BACKGROUND_DEPTH;pixels[y][x][0]=0.1f;pixels[y][x][1]=0.1f;pixels[y][x][2]=0.2f;}
    ocean_scene_init();ocean_scene_update(&s);raster_grid(ocean_sky,OCEAN_SKY_X,OCEAN_SKY_Y,0);raster_grid(ocean_water,OCEAN_X,OCEAN_Z,1);
    f=fopen(argv[1],"wb");if(!f)return 1;fprintf(f,"P6\n640 480\n255\n");
    for(y=0;y<480;++y)for(x=0;x<640;++x){unsigned c;for(c=0;c<3;++c)rgb[c]=(unsigned char)(pixels[y][x][c]*255+0.5f);fwrite(rgb,1,3,f);}
    fclose(f);printf("preview: %s; host raster, no PVR bloom or hardware timing\n",argv[1]);return 0;
}
#endif
