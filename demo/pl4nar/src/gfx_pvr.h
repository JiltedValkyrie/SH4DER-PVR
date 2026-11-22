/* Dreamcast back end extras: timing the platform shows in the overlay. */
#ifndef GFX_PVR_H
#define GFX_PVR_H
#include <stdint.h>
uint64_t gfx_pvr_wait_us(void);      /* time spent waiting for the PVR since the last call (then reset) */
uint32_t gfx_pvr_rtt_us(void);       /* the reflection pass's render time on the PVR, sampled one frame in 32 */
uint32_t gfx_pvr_bad_binds(void);    /* headers refused (wrong list / never compiled): must stay 0 */
#endif
