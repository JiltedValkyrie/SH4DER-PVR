/* Dreamcast entry: pad -> demo input, frame timing, the numbers the overlay shows.
 * KOS path: maple controller status (maple_enum_type + maple_dev_status), timer_us_gettime64, pvr_get_stats,
 * pvr_mem_available, mallinfo + sbrk for free main memory. No per-frame printf (serial is 115200 baud). */
#include <kos.h>
#include <malloc.h>
#include <unistd.h>
#include <arch/arch.h>
#include <arch/timer.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/pvr.h>
#include "demo.h"
#include "gfx.h"
#include "gfx_pvr.h"
#include "version.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

uint64_t plat_us(void) { return timer_us_gettime64(); }

static uint32_t ram_free_kb(void) {
    const struct mallinfo mi = mallinfo();
    const uintptr_t heap_end = (uintptr_t)sbrk(0), heap_top = (uintptr_t)_arch_mem_top - THD_KERNEL_STACK_SIZE;
    return (uint32_t)(((uint32_t)mi.fordblks + (heap_top > heap_end ? heap_top - heap_end : 0u)) / 1024u);
}

static void read_pad(demo_input_t *in, uint32_t *held) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    uint32_t now = 0u;
    memset(in, 0, sizeof *in);
    if (!st) { *held = 0u; return; }
    in->steer = (float)st->joyx * (1.0f / 127.0f);
    if (in->steer > -0.12f && in->steer < 0.12f) in->steer = 0.0f;
    if (st->buttons & CONT_DPAD_LEFT)  in->steer = -1.0f;
    if (st->buttons & CONT_DPAD_RIGHT) in->steer = 1.0f;
    in->gas = (float)st->rtrig * (1.0f / 255.0f);            /* right trigger: engine */
    in->brake = (float)st->ltrig * (1.0f / 255.0f);          /* left trigger: brake, then reverse */
    if (st->buttons & CONT_B) { in->orbit = in->steer; in->steer = 0.0f; }   /* B held: the stick turns the camera round the car */
    if (st->buttons & CONT_X)         now |= DEMO_BTN_REFLECT;
    if (st->buttons & CONT_Y)         now |= DEMO_BTN_TOD;
    if (st->buttons & CONT_A)         now |= (st->buttons & CONT_B) ? DEMO_BTN_SIZE : DEMO_BTN_LOOK;
    if (st->buttons & CONT_DPAD_UP)   now |= (st->buttons & CONT_B) ? DEMO_BTN_LAMP_UP : DEMO_BTN_MORE;
    if (st->buttons & CONT_DPAD_DOWN) now |= (st->buttons & CONT_B) ? DEMO_BTN_LAMP_DN : DEMO_BTN_LESS;
    if (st->buttons & CONT_START)     now |= DEMO_BTN_INFO;
    in->pressed = now & ~*held;
    *held = now;
}

int main(int argc, char **argv) {
    demo_stats_t st;
    demo_input_t in;
    uint32_t held = 0u, frame = 0u;
    uint64_t t_prev;
    (void)argc; (void)argv;
    printf("pl4nar reflection demo v%s b%d\n", DEMO_VERSION, DEMO_BUILD);
    gfx_init();
    demo_init();
    memset(&st, 0, sizeof st);
    printf("video memory free after load: %lu KB, main memory free: %lu KB\n", (unsigned long)(pvr_mem_available() / 1024u), (unsigned long)ram_free_kb());
    t_prev = timer_us_gettime64();
    for (;;) {
        const uint64_t t0 = timer_us_gettime64();
        float dt = (float)(uint32_t)(t0 - t_prev) * (1.0f / 1000000.0f);
        uint64_t t1, waited;
        pvr_stats_t ps;
        t_prev = t0;
        if (dt > 0.05f) dt = 0.05f;
        read_pad(&in, &held);
        demo_update(&in, dt);
        (void)gfx_pvr_wait_us();
        demo_draw(&st);
        t1 = timer_us_gettime64();
        waited = gfx_pvr_wait_us();
        st.cpu_us = (uint32_t)(t1 - t0 > waited ? t1 - t0 - waited : 0u);
        if (pvr_get_stats(&ps) == 0) {
            st.pvr_us = (uint32_t)(ps.rnd_last_time / 1000u);
            st.fps_x10 = (uint32_t)(ps.frame_rate * 10.0f);
        }
        st.rtt_us = demo_reflect_on() ? gfx_pvr_rtt_us() : 0u;
        if ((frame++ & 15u) == 0u) {                 /* the two memory numbers: 4 times a second is plenty */
            st.ram_free_kb = ram_free_kb();
            st.vram_free_kb = (uint32_t)(pvr_mem_available() / 1024u);
        }
    }
    return 0;
}
