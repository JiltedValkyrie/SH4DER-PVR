/* SH4ZAM-DCE tests: what every <topic>_check.c shares

   sh4_check.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   A <topic>_check.c is a host program: it loads the SH-4 ELF made from <topic>_kernels.c, fills records and buffers in the interpreter's
   RAM, calls the SH-4 functions and compares what they left. These are its few shared lines.
*/

#ifndef SH4_CHECK_H
#define SH4_CHECK_H

#include <stdlib.h>

#include "sh4run.h"
#include "shz_dce_test.h"

static sh4_t *S;

/* simulated RAM: `bytes` at an address `off` past a multiple of `align` (align 8, off 4: on a multiple of 4 and deliberately not of 8).
   Give every buffer the WEAKEST alignment its routine's comment allows: the interpreter then proves the routine needs no more. */
static inline uint32_t ram(uint32_t bytes, uint32_t align, uint32_t off) {
    const uint32_t a = sh4_alloc(S, bytes + off, align);
    if (!a) { printf("FAIL: out of simulated RAM\n"); exit(1); }
    return a + off;
}

static inline void *host(uint32_t addr, uint32_t bytes) { return sh4_ptr(S, addr, bytes); }

/* call an SH-4 function with one address (its record); a fault of the interpreter is a failed check */
static inline void call(const char *name, uint32_t job) {
    t_checks++;
    if (sh4_call(S, sh4_sym(S, name), job, 0, 0, 0) != 0) {
        if (t_fails++ < 20) printf("FAIL %s: %s\n", name, sh4_error(S));
    }
}

/* load the ELF named on the command line; 0: ready */
static inline int check_open(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <kernels.elf>\n", argv[0]); return 2; }
    S = sh4_new();
    if (!S || sh4_load_elf(S, argv[1]) != 0) { printf("FAIL: load: %s\n", S ? sh4_error(S) : "no memory"); return 1; }
    printf("vector unit: %s\n", sh4_hw_model() ? "bit-exact models" : "plain sums (not the console's last bits)");
    return 0;
}

#endif /* SH4_CHECK_H */
