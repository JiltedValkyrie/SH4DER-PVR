/* Regression: sky/sun must survive KOS background depth; haze meets horizon. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../demo/ocean_scene.h"
#define OCEAN_SKY_CHUNK_TEST 8
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    ocean_settings_t s={6,0,-0.045f,1,1,1,1,0,0};
    float sun[3],scaled[3],horizon[3],upper[3],fog[129];
    const float horizon_ray[3]={0,0,1},upper_ray[3]={0,0.6f,0.8f};
    unsigned i;
    ocean_scene_init();ocean_scene_update(&s);
    for(i=0;i<(OCEAN_SKY_X+1)*(OCEAN_SKY_Y+1);++i)assert(ocean_sky[i].z>OCEAN_BACKGROUND_DEPTH);
    for(i=0;i<(OCEAN_X+1)*(OCEAN_Z+1);++i)assert(ocean_water[i].z>OCEAN_SKY_DEPTH);
    ocean_sky_rgb(ocean_sun,sun);assert(sun[0]>0.98f&&sun[1]>0.88f);
    for(i=0;i<3;++i)scaled[i]=ocean_sun[i]*0.9995f;
    ocean_sky_rgb(scaled,sun);assert(sun[0]>0.98f&&sun[1]>0.88f);
    ocean_sky_rgb(horizon_ray,horizon);ocean_sky_rgb(upper_ray,upper);
    for(i=0;i<3;++i)assert(horizon[i]==ocean_fog_rgb[i]);
    assert(horizon[0]>upper[0]+0.3f&&upper[2]>upper[0]);
    ocean_fog_samples(fog);assert(fog[0]>0.999f&&fog[128]==0);
    for(i=1;i<=128;++i)assert(fog[i]>=0&&fog[i]<=fog[i-1]);
    assert(ocean_fog_amount(10)==0&&ocean_fog_amount(1000)==1);
    puts("ocean_scene: PASS background depth, sun/FSRRA tolerance, dusk gradient, matching horizon, fog table");
    /* QUALITY preset: the original mesh and the original sun are the start state, bit for bit */
    assert(ocean_mesh_w==OCEAN_X&&ocean_mesh_h==OCEAN_Z&&ocean_eye==OCEAN_EYE&&ocean_row_scale==1.0f);
    { unsigned c,r;ocean_mesh_grid(OCEAN_MESH_ORIGINAL,&c,&r);assert(c==OCEAN_X&&r==OCEAN_Z&&ocean_mesh_target(OCEAN_MESH_ORIGINAL)==2u*OCEAN_X*OCEAN_Z); }
    assert(!memcmp(ocean_sun,ocean_sun_original,sizeof(ocean_sun)));
    ocean_sun_vector(OCEAN_SUN_AZ_ORIGINAL,OCEAN_SUN_EL_ORIGINAL,sun);
    assert(!memcmp(sun,ocean_sun_original,sizeof(sun)));
    /* the original step values ARE the original vector: azimuth 9.806, elevation 3.440 deg */
    { const float rad=3.14159265f/180;
      assert(fabsf(sinf(3.440f*rad)-ocean_sun_original[1])<2e-4f&&fabsf(cosf(3.440f*rad)*sinf(9.806f*rad)-ocean_sun_original[0])<2e-4f);
      assert(fabsf(ocean_sun_el_deg[OCEAN_SUN_EL_ORIGINAL]-3.440f)<1e-6f&&fabsf(ocean_sun_az_deg[OCEAN_SUN_AZ_ORIGINAL]-9.806f)<1e-6f); }
    /* every sun step: a unit vector above the horizon (SH4DOW's sweep needs y > 0), azimuth to the right of the axis when positive */
    for(i=0;i<OCEAN_SUN_AZ_STEPS*OCEAN_SUN_EL_STEPS;++i){
        float v[3];unsigned az=i%OCEAN_SUN_AZ_STEPS,el=i/OCEAN_SUN_AZ_STEPS;
        ocean_sun_vector(az,el,v);
        assert(fabsf(ocean_dot(v,v)-1)<2e-3f&&v[1]>0.02f&&v[2]>0);
        assert((ocean_sun_az_deg[az]>0)==(v[0]>0)||ocean_sun_az_deg[az]==0);
        if(el)assert(v[1]>0);
    }
    /* the sky rows rebuilt in chunks are the init texture, and a different sun changes only the sun's region of the picture */
    { static uint16_t whole[OCEAN_ENV*OCEAN_ENV];unsigned y;
      memcpy(whole,ocean_environment,sizeof(whole));
      memset(ocean_environment,0,sizeof(ocean_environment));
      for(y=0;y<OCEAN_ENV;y+=OCEAN_SKY_CHUNK_TEST)ocean_env_rows(ocean_sun,y,y+OCEAN_SKY_CHUNK_TEST<OCEAN_ENV?y+OCEAN_SKY_CHUNK_TEST:OCEAN_ENV);
      assert(!memcmp(whole,ocean_environment,sizeof(whole)));
      ocean_sun_vector(OCEAN_SUN_AZ_STEPS-1,OCEAN_SUN_EL_STEPS-1,sun);
      ocean_env_rows(sun,0,OCEAN_ENV);
      assert(memcmp(whole,ocean_environment,sizeof(whole))); }
    /* every water mesh step (200 triangles apiece): the grid nearest the target at an aspect near the original's, ordered rows, water behind the sky */
    { float rows0[OCEAN_Z+1],cols0[OCEAN_X+1];unsigned m,prev=0;
      ocean_mesh_set(OCEAN_MESH_ORIGINAL);memcpy(rows0,ocean_rows,sizeof(rows0));memcpy(cols0,ocean_cols,sizeof(cols0));
      assert(OCEAN_MESH_STEP_FOR(2000)==8&&ocean_mesh_target(8)==2000&&ocean_mesh_target(0)==400&&ocean_mesh_target(OCEAN_MESH_ORIGINAL-1)==22800);
      for(m=0;m<OCEAN_MESH_STEPS;++m){
        unsigned c,r,tris,target=ocean_mesh_target(m);
        float aspect,miss;
        ocean_mesh_grid(m,&c,&r);tris=2*c*r;aspect=(float)c/(float)r;miss=((float)tris-(float)target)/(float)target;
        assert(c>=2&&r>=2&&c<=OCEAN_X&&r<=OCEAN_Z&&aspect>=1.10f&&aspect<=1.42f);
        assert(fabsf(miss)<=0.03f&&tris>prev); /* within 3 % of the step, and every step is more triangles than the last */
        prev=tris;
        if(m%9==0||m==OCEAN_MESH_ORIGINAL||m==8){
          unsigned x,z;
          ocean_mesh_set(m);
          assert(ocean_mesh_w==c&&ocean_mesh_h==r);
          for(z=0;z<ocean_mesh_h;++z)assert(ocean_rows[z+1]>ocean_rows[z]);
          assert(fabsf(ocean_rows[0]-3.2f)<1e-4f&&ocean_rows[ocean_mesh_h]>2700.0f&&ocean_rows[ocean_mesh_h]<2900.0f);
          assert(ocean_cols[0]<-0.76f&&ocean_cols[ocean_mesh_w]>0.76f);
          ocean_scene_update(&s);
          for(z=0;z<=ocean_mesh_h;++z)for(x=0;x<=ocean_mesh_w;++x){
            const ocean_vertex_t *v=&ocean_water[z*(ocean_mesh_w+1)+x];
            assert(v->z>OCEAN_SKY_DEPTH&&v->x>-1000&&v->x<1640&&v->y>-100&&v->y<580);
          }
          assert(ocean_water[0].y>ocean_water[ocean_mesh_h*(ocean_mesh_w+1)].y); /* nearer rows are lower on the screen */
          { static ocean_vertex_t a[(OCEAN_X+1)*(OCEAN_Z+1)];const unsigned n=(ocean_mesh_w+1)*(ocean_mesh_h+1);memcpy(a,ocean_water,sizeof(a));
            ocean_mesh_set(m);ocean_scene_update(&s);assert(!memcmp(a,ocean_water,n*sizeof(ocean_vertex_t))); } /* a rebuild on change is the whole of the change */
        }
      }
      ocean_mesh_set(OCEAN_MESH_ORIGINAL);
      assert(!memcmp(rows0,ocean_rows,sizeof(rows0))&&!memcmp(cols0,ocean_cols,sizeof(cols0))); }
    /* eye height: the original at 2.5 m, the clamps, the camera and sky untouched, the water's near edge below the picture at every height */
    { const float eyes[6]={0.0f,0.8f,1.5f,2.5f,5.0f,99.0f};unsigned k;
      static ocean_vertex_t a[(OCEAN_X+1)*(OCEAN_Z+1)],b[(OCEAN_X+1)*(OCEAN_Z+1)];static ocean_vertex_t sky0[(OCEAN_SKY_X+1)*(OCEAN_SKY_Y+1)];
      ocean_eye_set(OCEAN_EYE);ocean_scene_update(&s);memcpy(a,ocean_water,sizeof(a));memcpy(sky0,ocean_sky,sizeof(sky0));
      for(k=0;k<6;++k){
        unsigned x,z;
        ocean_eye_set(eyes[k]);
        assert(ocean_eye>=OCEAN_EYE_MIN&&ocean_eye<=OCEAN_EYE_MAX&&(eyes[k]<OCEAN_EYE_MIN?ocean_eye==OCEAN_EYE_MIN:eyes[k]>OCEAN_EYE_MAX?ocean_eye==OCEAN_EYE_MAX:ocean_eye==eyes[k]));
        assert(ocean_row_scale==(ocean_eye<OCEAN_EYE?ocean_eye*(1.0f/OCEAN_EYE):1.0f));
        ocean_scene_update(&s);
        assert(!memcmp(sky0,ocean_sky,sizeof(sky0))); /* sky mesh: the same at every height */
        for(z=0;z<=ocean_mesh_h;++z)for(x=0;x<=ocean_mesh_w;++x){
          const ocean_vertex_t *v=&ocean_water[z*(ocean_mesh_w+1)+x];
          assert(v->z>OCEAN_SKY_DEPTH&&v->x>-1000&&v->x<1640&&v->y>-100&&v->y<5000);
        }
        assert(ocean_water[0].y>=470.0f); /* nearest water row at or below the screen's bottom edge: no gap of background */
        assert(ocean_water[ocean_mesh_h*(ocean_mesh_w+1)].y<260.0f&&ocean_water[ocean_mesh_h*(ocean_mesh_w+1)].y>200.0f); /* the far edge at the horizon, 1.4 px under at 8 m */
        if(eyes[k]==OCEAN_EYE){ memcpy(b,ocean_water,sizeof(b)); assert(!memcmp(a,b,sizeof(a))); }
      }
      ocean_eye_set(OCEAN_EYE);assert(ocean_eye==OCEAN_EYE&&ocean_row_scale==1.0f); }
    puts("ocean_scene: PASS presets: original mesh, sun and eye by default, sun steps, chunked sky rebuild, every water triangle step, eye height");
    return 0;
}
