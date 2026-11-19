/* Regression: sky/sun must survive KOS background depth; haze meets horizon. */
#include <assert.h>
#include <stdio.h>
#include "../demo/ocean_scene.h"
int main(void){
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
    return 0;
}
