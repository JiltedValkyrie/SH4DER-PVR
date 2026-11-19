/* SH4ZAM-DCE test tool: sh4run from the command line

   sh4run_cli.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   sh4run_cli --mn <elf>                      the mnemonic of every 16-bit word of the executable sections (for the decoder check against objdump)
   sh4run_cli --call <elf> <symbol> [a0..a3]  call a function with integer arguments (decimal or 0x hex), print r0, fr0 and the counters
   sh4run_cli --trace <elf> <symbol> [a0..a3] the same, every instruction printed with the cycle it issued in
*/

#include "sh4run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_mn(uint32_t addr, uint16_t op, void *user) {
    (void)user;
    printf("%x %s\n", (unsigned)addr, sh4_mnemonic(op));
}

int main(int argc, char **argv) {
    sh4_t *s;
    uint32_t a[4] = { 0, 0, 0, 0 };
    const sh4_stats_t *st;
    int k, rc;
    if (argc < 3 || (strcmp(argv[1], "--mn") && argc < 4)) {
        printf("usage: sh4run_cli --mn <elf> | --call <elf> <symbol> [a0..a3] | --trace <elf> <symbol> [a0..a3]\n");
        return 2;
    }
    s = sh4_new();
    if (!s || sh4_load_elf(s, argv[2]) != 0) { printf("load failed: %s\n", s ? sh4_error(s) : "no memory"); return 1; }
    if (!strcmp(argv[1], "--mn")) { sh4_each_opcode(s, print_mn, NULL); return 0; }
    for (k = 0; k < 4 && 4 + k < argc; k++) a[k] = (uint32_t)strtoul(argv[4 + k], NULL, 0);
    sh4_trace(s, !strcmp(argv[1], "--trace"));
    rc = sh4_call(s, sh4_sym(s, argv[3]), a[0], a[1], a[2], a[3]);
    st = sh4_stats(s);
    if (rc) { printf("FAILED: %s\n", sh4_error(s)); return 1; }
    printf("r0=%08x fr0=%g instrs=%llu cycles=%llu (ESTIMATE) fipr=%u ftrv=%u fsrra=%u fsca=%u fmac=%u fmul=%u fadd=%u fsub=%u fdiv=%u fsqrt=%u fcmp=%u loads=%u stores=%u taken=%u\n",
           (unsigned)sh4_r0(s), (double)sh4_fr0(s), (unsigned long long)st->instrs, (unsigned long long)st->cycles, (unsigned)st->fipr, (unsigned)st->ftrv,
           (unsigned)st->fsrra, (unsigned)st->fsca, (unsigned)st->fmac, (unsigned)st->fmul, (unsigned)st->fadd, (unsigned)st->fsub, (unsigned)st->fdiv,
           (unsigned)st->fsqrt, (unsigned)st->fcmp, (unsigned)st->loads, (unsigned)st->stores, (unsigned)st->taken);
    return 0;
}
