/* Dreamcast back end extras: timing the platform shows in the overlay. */
#ifndef GFX_PVR_H
#define GFX_PVR_H
#include <stdint.h>
uint64_t gfx_pvr_wait_us(void);      /* time spent waiting for the PVR since the last call (then reset) */
uint32_t gfx_pvr_rtt_us(void);       /* the reflection pass's render time on the PVR, on the last sampled frame */
typedef struct gfx_pvr_pass {        /* one scene of a sampled frame, read after its render was waited for */
    uint32_t ta_bytes;               /* vertex buffer bytes this scene (pvr_stats_t vtx_buffer_used) */
    uint32_t ta_max;                 /* KallistiOS's largest scene so far, any scene (vtx_buffer_used_max) */
    uint32_t rnd_us;                 /* its render on the PVR (rnd_last_time) */
    uint32_t wait_us;                /* ready .. render done, timed here */
    uint32_t n;                      /* scenes sampled */
} gfx_pvr_pass_t;
void gfx_pvr_sample(int on);         /* 1: this frame waits for and reads both scenes; 2: it waits for its last render only
                                        (the frame before a sampled one); 0: neither. Call before the frame's draw. */
uint32_t gfx_pvr_timeouts(void);     /* waits given up (200 ms each): a hung PVR shows here, not as a hang */
const gfx_pvr_pass_t *gfx_pvr_pass(unsigned which);   /* 0 the screen scene, 1 the mirror scene */
uint32_t gfx_pvr_bad_binds(void);    /* headers refused (wrong list / never compiled): must stay 0 */
#endif
