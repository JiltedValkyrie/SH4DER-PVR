/* Water derivatives, optical limits, sampling filter and ridge occlusion. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "sh4der_water.h"
#include "sh4dow_heightfield.h"
static int closef(float a,float b,float tol){return fabsf(a-b)<tol;}
int main(void){
    sh4der_water_wave_t w={0.6f,0.8f,0.2f,7.0f,2.0f,0.4f,0};
    sh4der_water_surface_t s,lo,hi;float eps=0.002f,n[3],r[3],v[3]={0,1,0};
    float incident[3]={1,1,1},absorb[3]={0.5f,0.1f,0.05f},out[3];
    unsigned i,x,z;float h[9*9]={0},shade[9*9],scratch[18],sun[3]={0,0.5f,1};
    s=sh4der_water_sample(&w,1,0.7f,1.2f,0.3f,0,1,1);
    lo=sh4der_water_sample(&w,1,0.7f-eps,1.2f,0.3f,0,1,1);hi=sh4der_water_sample(&w,1,0.7f+eps,1.2f,0.3f,0,1,1);
    assert(closef(s.dx,(hi.height-lo.height)/(2*eps),0.002f));
    lo=sh4der_water_sample(&w,1,0.7f,1.2f-eps,0.3f,0,1,1);hi=sh4der_water_sample(&w,1,0.7f,1.2f+eps,0.3f,0,1,1);
    assert(closef(s.dz,(hi.height-lo.height)/(2*eps),0.002f));
    s=sh4der_water_sample(&w,1,1,2,3,10,1,1);assert(s.height==0&&s.dx==0&&s.dz==0);
    w.ripple=1;s=sh4der_water_sample(&w,1,1,2,3,0,1,1);assert(s.height==0);assert(fabsf(s.dx)+fabsf(s.dz)>0.001f);
    s.dx=s.dz=0;sh4der_water_normal(s,n);sh4der_water_reflect(n,v,r);assert(closef(r[1],1,0.0005f));
    assert(closef(sh4der_water_fresnel(1),0.0204f,0.0001f));assert(closef(sh4der_water_fresnel(0),1,0.0001f));
    for(i=0;i<=100;++i){float f=sh4der_water_fresnel(i*0.01f);assert(f>=0.0203f&&f<=1.0001f);}
    sh4der_water_transmit(incident,absorb,0,out);assert(closef(out[0],1,0.00002f));assert(sh4der_water_exp(0)==1.0f);assert(closef(sh4der_water_exp(-0.012f),expf(-0.012f),1e-6f));
    sh4der_water_transmit(incident,absorb,5,out);assert(out[0]<out[1]&&out[1]<out[2]);
    assert(sh4dow_heightfield_sun(h,9,9,1,sun,0.001f,0.05f,shade,scratch));
    for(i=0;i<81;++i)assert(shade[i]==0);
    for(x=0;x<9;++x)h[6*9+x]=2;
    sh4dow_heightfield_sun(h,9,9,1,sun,0.001f,0.05f,shade,scratch);
    assert(shade[5*9+4]>0.99f&&shade[3*9+4]>0.99f);assert(shade[1*9+4]==0&&shade[7*9+4]==0);
    sun[2]=-1;sh4dow_heightfield_sun(h,9,9,1,sun,0.001f,0.05f,shade,scratch);
    assert(shade[7*9+4]>0.99f&&shade[5*9+4]==0);
    for(i=0;i<81;++i)h[i]=0;
    for(z=0;z<9;++z)h[z*9+6]=2;
    sun[0]=1;sun[2]=0;
    sh4dow_heightfield_sun(h,9,9,1,sun,0.001f,0.05f,shade,scratch);
    assert(shade[4*9+5]>0.99f&&shade[4*9+1]==0);
    sun[0]=0;sh4dow_heightfield_sun(h,9,9,1,sun,0.001f,0.05f,shade,scratch);for(i=0;i<81;++i)assert(shade[i]==0);
    assert(!sh4dow_heightfield_sun(h,9,9,0,sun,0,1,shade,scratch));
    puts("water: PASS derivatives, filtering, Fresnel, transmission, flat/ridge/reversed/transposed/vertical shadows");return 0;
}
