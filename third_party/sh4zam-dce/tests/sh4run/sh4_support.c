/* SH4ZAM-DCE test tool: what a bare SH-4 test program needs from a C library and does not get from one

   sh4_support.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   The test ELFs are linked without KallistiOS (-nostartfiles): a failed assert has nowhere to print. It stores to an address outside the
   RAM instead, which stops the interpreter with the address of the assert in its message.
*/

#include <stdint.h>

static void sh4_support_stop(uint32_t code) {
    *(volatile uint32_t *)(uintptr_t)(0xa55e0000u + (code & 0xfffcu)) = code;   /* "outside the RAM": the run stops here */
    for (;;) { }
}

void __assert(const char *file, int line, const char *expr, const char *msg, const char *func) {
    (void)file; (void)expr; (void)msg; (void)func;
    sh4_support_stop((uint32_t)line << 2);
}

void __assert_func(const char *file, int line, const char *func, const char *expr) {
    (void)file; (void)func; (void)expr;
    sh4_support_stop((uint32_t)line << 2);
}

void abort(void) {
    sh4_support_stop(0u);
}
