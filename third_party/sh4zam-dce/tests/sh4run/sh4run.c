/* SH4ZAM-DCE test tool: sh4run

   sh4run.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   The interpreter behind sh4run.h. Three parts: the decoder (one table of bit patterns, the SH-4
   manual's own spelling), the executor (one case an instruction), and the timing model.

   FIPR, FTRV, FSRRA, FSCA. Build with -DSH4RUN_HW_MODEL and the include path of sh4_fpu_approx.h
   (bit-exact models of the SH-4's vector unit, MIT, Copyright 2026 Stefanos Kornilios Mitsis
   Poiitidis, checked by its author against a Dreamcast) and these four give the console's own
   bits. Without it they are plain sums and 1 / sqrtf: right to a few units in the last place,
   not the console's bits.

   FMAC is one fused multiply-add (fmaf), as the manual's operation describes it.

   The timing model (ESTIMATE). From the SH7750 manual's pipeline section: two instructions issue
   a cycle when their groups allow it (MT with anything, EX / BR / LS / FE with any other group,
   CO alone); an instruction waits for its source registers (each result is ready `latency` cycles
   after its instruction issued; latency 0 can be used in the same cycle); a compare of the MT
   group feeds a branch in the same cycle; a taken branch costs 2 cycles (3 through a register);
   FTRV keeps the FPU for 4 cycles and gives its four results after 5, 5, 6, 7; FIPR after 4;
   FADD / FSUB / FMUL / FMAC / FLOAT / FTRC after 3; FCMP sets T after 2; FDIV after 12, FSQRT 11
   (the divider is busy 10 and 9, other FPU work goes on); loads after 2; FTRC feeds STS FPUL after
   1 (GCC's sh4.md bypass). The SH7750 manual lists neither FSRRA nor FSCA: their latencies here are
   the SH-4A manual's (FSRRA 5, FSCA 7 for the cosine, 6 assumed for the sine), and the FPU is
   ASSUMED busy 3 and 4 cycles (three and four passes of the same multiplier as FIPR). Not
   modelled: cache misses, the write buffer, TLB, interrupts.
*/

#include "sh4run.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SH4RUN_HW_MODEL
#include "sh4_fpu_approx.h"
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "sh4run reads the simulated RAM with host loads: the host must be little-endian"
#endif

#define RETURN_MAGIC 0xfeed0000u    /* where a call returns to: outside the RAM, never fetched */
#define STACK_TOP (SH4_RAM_BASE + SH4_RAM_SIZE - 0x100u)
#define HEAP_BASE (SH4_RAM_BASE + 0x00800000u)   /* test data: the upper half, under the stack */
#define HEAP_END (SH4_RAM_BASE + SH4_RAM_SIZE - 0x00010000u)
#define FPSCR_INIT 0x00040000u      /* KallistiOS: DN = 1, round to nearest, single precision */
#define FPSCR_MODE 0x003c0003u      /* FR, SZ, PR, DN, RM: what a call must give back */
#define MAX_INSTRS 400000000ull
#define MAX_SYMS 4096
#define MAX_XSECS 16

enum { G_MT, G_EX, G_BR, G_LS, G_FE, G_CO };

enum {
    OP_BAD = 0,
    OP_MOVI, OP_MOVWI, OP_MOVLI, OP_MOV, OP_MOVBS, OP_MOVWS, OP_MOVLS, OP_MOVBL, OP_MOVWL, OP_MOVLL, OP_MOVBM, OP_MOVWM, OP_MOVLM,
    OP_MOVBP, OP_MOVWP, OP_MOVLP, OP_MOVBS4, OP_MOVWS4, OP_MOVLS4, OP_MOVBL4, OP_MOVWL4, OP_MOVLL4, OP_MOVBS0, OP_MOVWS0, OP_MOVLS0,
    OP_MOVBL0, OP_MOVWL0, OP_MOVLL0, OP_MOVBSG, OP_MOVWSG, OP_MOVLSG, OP_MOVBLG, OP_MOVWLG, OP_MOVLLG, OP_MOVA, OP_MOVT, OP_SWAPB,
    OP_SWAPW, OP_XTRCT, OP_PREF, OP_MOVCAL, OP_OCB,
    OP_ADD, OP_ADDI, OP_ADDC, OP_ADDV, OP_CMPIM, OP_CMPEQ, OP_CMPHS, OP_CMPGE, OP_CMPHI, OP_CMPGT, OP_CMPPL, OP_CMPPZ, OP_CMPSTR,
    OP_DIV1, OP_DIV0S, OP_DIV0U, OP_DMULS, OP_DMULU, OP_DT, OP_EXTSB, OP_EXTSW, OP_EXTUB, OP_EXTUW, OP_MULL, OP_MULS, OP_MULU,
    OP_NEG, OP_NEGC, OP_SUB, OP_SUBC, OP_SUBV,
    OP_AND, OP_ANDI, OP_NOT, OP_OR, OP_ORI, OP_TST, OP_TSTI, OP_XOR, OP_XORI,
    OP_ROTL, OP_ROTR, OP_ROTCL, OP_ROTCR, OP_SHAD, OP_SHAL, OP_SHAR, OP_SHLD, OP_SHLL, OP_SHLL2, OP_SHLL8, OP_SHLL16, OP_SHLR,
    OP_SHLR2, OP_SHLR8, OP_SHLR16,
    OP_BF, OP_BFS, OP_BT, OP_BTS, OP_BRA, OP_BRAF, OP_BSR, OP_BSRF, OP_JMP, OP_JSR, OP_RTS,
    OP_CLRMAC, OP_CLRS, OP_CLRT, OP_NOP, OP_SETS, OP_SETT, OP_LDCGBR, OP_STCGBR,
    OP_LDSMACH, OP_LDSMACL, OP_LDSPR, OP_LDSMMACH, OP_LDSMMACL, OP_LDSMPR, OP_STSMACH, OP_STSMACL, OP_STSPR, OP_STSMMACH,
    OP_STSMMACL, OP_STSMPR, OP_LDSFPUL, OP_LDSFPSCR, OP_LDSMFPUL, OP_LDSMFPSCR, OP_STSFPUL, OP_STSFPSCR, OP_STSMFPUL, OP_STSMFPSCR,
    OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV, OP_FCMPEQ, OP_FCMPGT, OP_FMOVIL, OP_FMOVIS, OP_FMOVL, OP_FMOVP, OP_FMOVS, OP_FMOVM, OP_FMOV,
    OP_FSTS, OP_FLDS, OP_FLOAT, OP_FTRC, OP_FNEG, OP_FABS, OP_FSQRT, OP_FSRRA, OP_FLDI0, OP_FLDI1, OP_FCNVSD, OP_FCNVDS, OP_FIPR,
    OP_FTRV, OP_FSCA, OP_FSCHG, OP_FRCHG, OP_FMAC,
    OP_COUNT
};

typedef struct { const char *pat; const char *name; uint8_t id, grp, rate; } opdef_t;

/* The SH-4 manual's bit patterns (n, m: register fields; d, i: displacement, immediate), GNU objdump's mnemonics, the pipeline group and,
   for the CO group, the cycles the instruction takes to issue. The first pattern that fits wins. */
static const opdef_t opdefs[] = {
    { "1110nnnniiiiiiii", "mov", OP_MOVI, G_MT, 1 },        { "1001nnnndddddddd", "mov.w", OP_MOVWI, G_LS, 1 },
    { "1101nnnndddddddd", "mov.l", OP_MOVLI, G_LS, 1 },     { "0110nnnnmmmm0011", "mov", OP_MOV, G_MT, 1 },
    { "0010nnnnmmmm0000", "mov.b", OP_MOVBS, G_LS, 1 },     { "0010nnnnmmmm0001", "mov.w", OP_MOVWS, G_LS, 1 },
    { "0010nnnnmmmm0010", "mov.l", OP_MOVLS, G_LS, 1 },     { "0110nnnnmmmm0000", "mov.b", OP_MOVBL, G_LS, 1 },
    { "0110nnnnmmmm0001", "mov.w", OP_MOVWL, G_LS, 1 },     { "0110nnnnmmmm0010", "mov.l", OP_MOVLL, G_LS, 1 },
    { "0010nnnnmmmm0100", "mov.b", OP_MOVBM, G_LS, 1 },     { "0010nnnnmmmm0101", "mov.w", OP_MOVWM, G_LS, 1 },
    { "0010nnnnmmmm0110", "mov.l", OP_MOVLM, G_LS, 1 },     { "0110nnnnmmmm0100", "mov.b", OP_MOVBP, G_LS, 1 },
    { "0110nnnnmmmm0101", "mov.w", OP_MOVWP, G_LS, 1 },     { "0110nnnnmmmm0110", "mov.l", OP_MOVLP, G_LS, 1 },
    { "10000000nnnndddd", "mov.b", OP_MOVBS4, G_LS, 1 },    { "10000001nnnndddd", "mov.w", OP_MOVWS4, G_LS, 1 },
    { "0001nnnnmmmmdddd", "mov.l", OP_MOVLS4, G_LS, 1 },    { "10000100mmmmdddd", "mov.b", OP_MOVBL4, G_LS, 1 },
    { "10000101mmmmdddd", "mov.w", OP_MOVWL4, G_LS, 1 },    { "0101nnnnmmmmdddd", "mov.l", OP_MOVLL4, G_LS, 1 },
    { "0000nnnnmmmm0100", "mov.b", OP_MOVBS0, G_LS, 1 },    { "0000nnnnmmmm0101", "mov.w", OP_MOVWS0, G_LS, 1 },
    { "0000nnnnmmmm0110", "mov.l", OP_MOVLS0, G_LS, 1 },    { "0000nnnnmmmm1100", "mov.b", OP_MOVBL0, G_LS, 1 },
    { "0000nnnnmmmm1101", "mov.w", OP_MOVWL0, G_LS, 1 },    { "0000nnnnmmmm1110", "mov.l", OP_MOVLL0, G_LS, 1 },
    { "11000000dddddddd", "mov.b", OP_MOVBSG, G_LS, 1 },    { "11000001dddddddd", "mov.w", OP_MOVWSG, G_LS, 1 },
    { "11000010dddddddd", "mov.l", OP_MOVLSG, G_LS, 1 },    { "11000100dddddddd", "mov.b", OP_MOVBLG, G_LS, 1 },
    { "11000101dddddddd", "mov.w", OP_MOVWLG, G_LS, 1 },    { "11000110dddddddd", "mov.l", OP_MOVLLG, G_LS, 1 },
    { "11000111dddddddd", "mova", OP_MOVA, G_EX, 1 },       { "0000nnnn00101001", "movt", OP_MOVT, G_EX, 1 },
    { "0110nnnnmmmm1000", "swap.b", OP_SWAPB, G_EX, 1 },    { "0110nnnnmmmm1001", "swap.w", OP_SWAPW, G_EX, 1 },
    { "0010nnnnmmmm1101", "xtrct", OP_XTRCT, G_EX, 1 },     { "0000nnnn10000011", "pref", OP_PREF, G_LS, 1 },
    { "0000nnnn11000011", "movca.l", OP_MOVCAL, G_LS, 1 },  { "0000nnnn10010011", "ocbi", OP_OCB, G_LS, 1 },
    { "0000nnnn10100011", "ocbp", OP_OCB, G_LS, 1 },        { "0000nnnn10110011", "ocbwb", OP_OCB, G_LS, 1 },

    { "0011nnnnmmmm1100", "add", OP_ADD, G_EX, 1 },         { "0111nnnniiiiiiii", "add", OP_ADDI, G_EX, 1 },
    { "0011nnnnmmmm1110", "addc", OP_ADDC, G_EX, 1 },       { "0011nnnnmmmm1111", "addv", OP_ADDV, G_EX, 1 },
    { "10001000iiiiiiii", "cmp/eq", OP_CMPIM, G_MT, 1 },    { "0011nnnnmmmm0000", "cmp/eq", OP_CMPEQ, G_MT, 1 },
    { "0011nnnnmmmm0010", "cmp/hs", OP_CMPHS, G_MT, 1 },    { "0011nnnnmmmm0011", "cmp/ge", OP_CMPGE, G_MT, 1 },
    { "0011nnnnmmmm0110", "cmp/hi", OP_CMPHI, G_MT, 1 },    { "0011nnnnmmmm0111", "cmp/gt", OP_CMPGT, G_MT, 1 },
    { "0100nnnn00010101", "cmp/pl", OP_CMPPL, G_MT, 1 },    { "0100nnnn00010001", "cmp/pz", OP_CMPPZ, G_MT, 1 },
    { "0010nnnnmmmm1100", "cmp/str", OP_CMPSTR, G_MT, 1 },  { "0011nnnnmmmm0100", "div1", OP_DIV1, G_EX, 1 },
    { "0010nnnnmmmm0111", "div0s", OP_DIV0S, G_EX, 1 },     { "0000000000011001", "div0u", OP_DIV0U, G_EX, 1 },
    { "0011nnnnmmmm1101", "dmuls.l", OP_DMULS, G_CO, 2 },   { "0011nnnnmmmm0101", "dmulu.l", OP_DMULU, G_CO, 2 },
    { "0100nnnn00010000", "dt", OP_DT, G_EX, 1 },           { "0110nnnnmmmm1110", "exts.b", OP_EXTSB, G_MT, 1 },
    { "0110nnnnmmmm1111", "exts.w", OP_EXTSW, G_MT, 1 },    { "0110nnnnmmmm1100", "extu.b", OP_EXTUB, G_MT, 1 },
    { "0110nnnnmmmm1101", "extu.w", OP_EXTUW, G_MT, 1 },    { "0000nnnnmmmm0111", "mul.l", OP_MULL, G_CO, 2 },
    { "0010nnnnmmmm1111", "muls.w", OP_MULS, G_CO, 2 },     { "0010nnnnmmmm1110", "mulu.w", OP_MULU, G_CO, 2 },
    { "0110nnnnmmmm1011", "neg", OP_NEG, G_EX, 1 },         { "0110nnnnmmmm1010", "negc", OP_NEGC, G_EX, 1 },
    { "0011nnnnmmmm1000", "sub", OP_SUB, G_EX, 1 },         { "0011nnnnmmmm1010", "subc", OP_SUBC, G_EX, 1 },
    { "0011nnnnmmmm1011", "subv", OP_SUBV, G_EX, 1 },

    { "0010nnnnmmmm1001", "and", OP_AND, G_EX, 1 },         { "11001001iiiiiiii", "and", OP_ANDI, G_EX, 1 },
    { "0110nnnnmmmm0111", "not", OP_NOT, G_EX, 1 },         { "0010nnnnmmmm1011", "or", OP_OR, G_EX, 1 },
    { "11001011iiiiiiii", "or", OP_ORI, G_EX, 1 },          { "0010nnnnmmmm1000", "tst", OP_TST, G_MT, 1 },
    { "11001000iiiiiiii", "tst", OP_TSTI, G_MT, 1 },        { "0010nnnnmmmm1010", "xor", OP_XOR, G_EX, 1 },
    { "11001010iiiiiiii", "xor", OP_XORI, G_EX, 1 },

    { "0100nnnn00000100", "rotl", OP_ROTL, G_EX, 1 },       { "0100nnnn00000101", "rotr", OP_ROTR, G_EX, 1 },
    { "0100nnnn00100100", "rotcl", OP_ROTCL, G_EX, 1 },     { "0100nnnn00100101", "rotcr", OP_ROTCR, G_EX, 1 },
    { "0100nnnnmmmm1100", "shad", OP_SHAD, G_EX, 1 },       { "0100nnnn00100000", "shal", OP_SHAL, G_EX, 1 },
    { "0100nnnn00100001", "shar", OP_SHAR, G_EX, 1 },       { "0100nnnnmmmm1101", "shld", OP_SHLD, G_EX, 1 },
    { "0100nnnn00000000", "shll", OP_SHLL, G_EX, 1 },       { "0100nnnn00001000", "shll2", OP_SHLL2, G_EX, 1 },
    { "0100nnnn00011000", "shll8", OP_SHLL8, G_EX, 1 },     { "0100nnnn00101000", "shll16", OP_SHLL16, G_EX, 1 },
    { "0100nnnn00000001", "shlr", OP_SHLR, G_EX, 1 },       { "0100nnnn00001001", "shlr2", OP_SHLR2, G_EX, 1 },
    { "0100nnnn00011001", "shlr8", OP_SHLR8, G_EX, 1 },     { "0100nnnn00101001", "shlr16", OP_SHLR16, G_EX, 1 },

    { "10001011dddddddd", "bf", OP_BF, G_BR, 1 },           { "10001111dddddddd", "bf.s", OP_BFS, G_BR, 1 },
    { "10001001dddddddd", "bt", OP_BT, G_BR, 1 },           { "10001101dddddddd", "bt.s", OP_BTS, G_BR, 1 },
    { "1010dddddddddddd", "bra", OP_BRA, G_BR, 1 },         { "0000nnnn00100011", "braf", OP_BRAF, G_CO, 2 },
    { "1011dddddddddddd", "bsr", OP_BSR, G_BR, 1 },         { "0000nnnn00000011", "bsrf", OP_BSRF, G_CO, 2 },
    { "0100nnnn00101011", "jmp", OP_JMP, G_CO, 2 },         { "0100nnnn00001011", "jsr", OP_JSR, G_CO, 2 },
    { "0000000000001011", "rts", OP_RTS, G_CO, 2 },

    { "0000000000101000", "clrmac", OP_CLRMAC, G_CO, 1 },   { "0000000001001000", "clrs", OP_CLRS, G_CO, 1 },
    { "0000000000001000", "clrt", OP_CLRT, G_MT, 1 },       { "0000000000001001", "nop", OP_NOP, G_MT, 1 },
    { "0000000001011000", "sets", OP_SETS, G_CO, 1 },       { "0000000000011000", "sett", OP_SETT, G_MT, 1 },
    { "0100mmmm00011110", "ldc", OP_LDCGBR, G_CO, 3 },      { "0000nnnn00010010", "stc", OP_STCGBR, G_CO, 2 },
    { "0100mmmm00001010", "lds", OP_LDSMACH, G_CO, 1 },     { "0100mmmm00011010", "lds", OP_LDSMACL, G_CO, 1 },
    { "0100mmmm00101010", "lds", OP_LDSPR, G_CO, 2 },       { "0100mmmm00000110", "lds.l", OP_LDSMMACH, G_CO, 1 },
    { "0100mmmm00010110", "lds.l", OP_LDSMMACL, G_CO, 1 },  { "0100mmmm00100110", "lds.l", OP_LDSMPR, G_CO, 2 },
    { "0000nnnn00001010", "sts", OP_STSMACH, G_CO, 1 },     { "0000nnnn00011010", "sts", OP_STSMACL, G_CO, 1 },
    { "0000nnnn00101010", "sts", OP_STSPR, G_CO, 2 },       { "0100nnnn00000010", "sts.l", OP_STSMMACH, G_CO, 1 },
    { "0100nnnn00010010", "sts.l", OP_STSMMACL, G_CO, 1 },  { "0100nnnn00100010", "sts.l", OP_STSMPR, G_CO, 2 },
    { "0100mmmm01011010", "lds", OP_LDSFPUL, G_LS, 1 },     { "0100mmmm01101010", "lds", OP_LDSFPSCR, G_CO, 1 },
    { "0100mmmm01010110", "lds.l", OP_LDSMFPUL, G_CO, 1 },  { "0100mmmm01100110", "lds.l", OP_LDSMFPSCR, G_CO, 1 },
    { "0000nnnn01011010", "sts", OP_STSFPUL, G_LS, 1 },     { "0000nnnn01101010", "sts", OP_STSFPSCR, G_CO, 1 },
    { "0100nnnn01010010", "sts.l", OP_STSMFPUL, G_CO, 1 },  { "0100nnnn01100010", "sts.l", OP_STSMFPSCR, G_CO, 1 },

    { "1111001111111101", "fschg", OP_FSCHG, G_FE, 1 },     { "1111101111111101", "frchg", OP_FRCHG, G_FE, 1 },
    { "1111nnnnmmmm0000", "fadd", OP_FADD, G_FE, 1 },       { "1111nnnnmmmm0001", "fsub", OP_FSUB, G_FE, 1 },
    { "1111nnnnmmmm0010", "fmul", OP_FMUL, G_FE, 1 },       { "1111nnnnmmmm0011", "fdiv", OP_FDIV, G_FE, 1 },
    { "1111nnnnmmmm0100", "fcmp/eq", OP_FCMPEQ, G_FE, 1 },  { "1111nnnnmmmm0101", "fcmp/gt", OP_FCMPGT, G_FE, 1 },
    { "1111nnnnmmmm0110", "fmov", OP_FMOVIL, G_LS, 1 },     { "1111nnnnmmmm0111", "fmov", OP_FMOVIS, G_LS, 1 },
    { "1111nnnnmmmm1000", "fmov", OP_FMOVL, G_LS, 1 },      { "1111nnnnmmmm1001", "fmov", OP_FMOVP, G_LS, 1 },
    { "1111nnnnmmmm1010", "fmov", OP_FMOVS, G_LS, 1 },      { "1111nnnnmmmm1011", "fmov", OP_FMOVM, G_LS, 1 },
    { "1111nnnnmmmm1100", "fmov", OP_FMOV, G_LS, 1 },       { "1111nnnn00001101", "fsts", OP_FSTS, G_LS, 1 },
    { "1111mmmm00011101", "flds", OP_FLDS, G_LS, 1 },       { "1111nnnn00101101", "float", OP_FLOAT, G_FE, 1 },
    { "1111mmmm00111101", "ftrc", OP_FTRC, G_FE, 1 },       { "1111nnnn01001101", "fneg", OP_FNEG, G_LS, 1 },
    { "1111nnnn01011101", "fabs", OP_FABS, G_LS, 1 },       { "1111nnnn01101101", "fsqrt", OP_FSQRT, G_FE, 1 },
    { "1111nnnn01111101", "fsrra", OP_FSRRA, G_FE, 1 },     { "1111nnnn10001101", "fldi0", OP_FLDI0, G_LS, 1 },
    { "1111nnnn10011101", "fldi1", OP_FLDI1, G_LS, 1 },     { "1111nnnn10101101", "fcnvsd", OP_FCNVSD, G_FE, 1 },
    { "1111mmmm10111101", "fcnvds", OP_FCNVDS, G_FE, 1 },   { "1111nnmm11101101", "fipr", OP_FIPR, G_FE, 1 },
    { "1111nn0111111101", "ftrv", OP_FTRV, G_FE, 1 },       { "1111nnn011111101", "fsca", OP_FSCA, G_FE, 1 },
    { "1111nnnnmmmm1110", "fmac", OP_FMAC, G_FE, 1 },
};
#define NDEFS (sizeof opdefs / sizeof opdefs[0])

static uint8_t lut[65536];   /* opcode -> index into opdefs + 1, 0: unknown */
static int lut_ready;

static void lut_build(void) {
    static uint16_t mask[NDEFS], val[NDEFS];
    size_t k;
    uint32_t op;
    if (lut_ready)
        return;
    for (k = 0; k < NDEFS; k++) {
        int b;
        mask[k] = 0; val[k] = 0;
        for (b = 0; b < 16; b++) {
            const char c = opdefs[k].pat[b];
            if (c == '0' || c == '1') {
                mask[k] |= (uint16_t)(1u << (15 - b));
                if (c == '1') val[k] |= (uint16_t)(1u << (15 - b));
            }
        }
    }
    for (op = 0; op < 65536; op++) {
        lut[op] = 0;
        for (k = 0; k < NDEFS; k++)
            if ((op & mask[k]) == val[k]) { lut[op] = (uint8_t)(k + 1); break; }
    }
    lut_ready = 1;
}

const char *sh4_mnemonic(uint16_t opcode) {
    lut_build();
    return lut[opcode] ? opdefs[lut[opcode] - 1].name : "?";
}

typedef struct { uint32_t addr; char name[60]; } sym_t;

struct sh4 {
    uint8_t *ram;
    uint32_t r[16];
    uint32_t fp[2][16];          /* [bank][register], raw bits; the front bank is fp[FPSCR.FR] */
    uint32_t pc, pr, gbr, mach, macl, fpul, fpscr;
    uint32_t t, q, m, s;
    /* run state */
    int halt;
    char err[400];
    uint64_t limit;
    int trace, poison;
    uint32_t seed, calls;
    float fargs[8];
    int nfargs;
    uint32_t heap;
    /* timing */
    int64_t cyc;                 /* cycle the last instruction issued in */
    int slot;                    /* instructions issued in that cycle (2: full) */
    int prev_grp;
    int64_t fe_free, div_free, resume;
    int64_t rt_r[16], rt_f[2][16], rt_t, rt_fpul, rt_fpul_sts, rt_mac, rt_pr, rt_fpscr;
    int t_by_mt;
    int64_t need;
    struct { int64_t *p; int lat; } pd[24];
    int npd;
    int grp, rate;
    int64_t fe_hold, div_hold;
    /* program */
    sym_t *syms;
    int nsyms;
    struct { uint32_t addr, size; } xsec[MAX_XSECS];
    int nxsec;
    sh4_stats_t st;
};

static void fault(sh4_t *S, const char *fmt, ...) {
    va_list ap;
    size_t k;
    if (S->halt)
        return;
    S->halt = 1;
    k = (size_t)snprintf(S->err, sizeof S->err, "pc=%08x: ", (unsigned)S->pc);
    va_start(ap, fmt);
    vsnprintf(S->err + k, sizeof S->err - k, fmt, ap);
    va_end(ap);
}

static uint32_t rng(sh4_t *S) {
    uint32_t x = S->seed ? S->seed : 0x9e3779b9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    S->seed = x;
    return x;
}

/* ---- memory ------------------------------------------------------------------------------------------------------------------------ */

static uint8_t *mem(sh4_t *S, uint32_t a, uint32_t n, const char *what) {
    if ((a & (n - 1u)) != 0u) {
        fault(S, "ADDRESS ERROR: %u-byte %s at %08x is not aligned to %u (a console stops here)", (unsigned)n, what, (unsigned)a, (unsigned)n);
        return NULL;
    }
    if (a < SH4_RAM_BASE || a - SH4_RAM_BASE > SH4_RAM_SIZE - n) {
        fault(S, "%u-byte %s at %08x is outside the RAM", (unsigned)n, what, (unsigned)a);
        return NULL;
    }
    return S->ram + (a - SH4_RAM_BASE);
}

static uint32_t rd8(sh4_t *S, uint32_t a)  { const uint8_t *p = mem(S, a, 1, "read"); S->st.loads++; return p ? p[0] : 0u; }
static uint32_t rd16(sh4_t *S, uint32_t a) { const uint8_t *p = mem(S, a, 2, "read"); uint16_t v = 0; S->st.loads++; if (p) memcpy(&v, p, 2); return v; }
static uint32_t rd32(sh4_t *S, uint32_t a) { const uint8_t *p = mem(S, a, 4, "read"); uint32_t v = 0; S->st.loads++; if (p) memcpy(&v, p, 4); return v; }
static void wr8(sh4_t *S, uint32_t a, uint32_t v)  { uint8_t *p = mem(S, a, 1, "write"); S->st.stores++; if (p) p[0] = (uint8_t)v; }
static void wr16(sh4_t *S, uint32_t a, uint32_t v) { uint8_t *p = mem(S, a, 2, "write"); uint16_t w = (uint16_t)v; S->st.stores++; if (p) memcpy(p, &w, 2); }
static void wr32(sh4_t *S, uint32_t a, uint32_t v) { uint8_t *p = mem(S, a, 4, "write"); S->st.stores++; if (p) memcpy(p, &v, 4); }
/* a pair of singles, FPSCR.SZ = 1: one 8-byte access, the lower address is the even register's */
static void rd64(sh4_t *S, uint32_t a, uint32_t *lo, uint32_t *hi) {
    const uint8_t *p = mem(S, a, 8, "read");
    S->st.loads++;
    *lo = 0; *hi = 0;
    if (p) { memcpy(lo, p, 4); memcpy(hi, p + 4, 4); }
}
static void wr64(sh4_t *S, uint32_t a, uint32_t lo, uint32_t hi) {
    uint8_t *p = mem(S, a, 8, "write");
    S->st.stores++;
    if (p) { memcpy(p, &lo, 4); memcpy(p + 4, &hi, 4); }
}

/* ---- registers, with the time each becomes ready ------------------------------------------------------------------------------------- */

#define FRB(S) (((S)->fpscr >> 21) & 1u)
#define SZ(S) (((S)->fpscr >> 20) & 1u)
#define PR(S) (((S)->fpscr >> 19) & 1u)
#define DN(S) (((S)->fpscr >> 18) & 1u)

static void pend(sh4_t *S, int64_t *p, int lat) { S->pd[S->npd].p = p; S->pd[S->npd].lat = lat; S->npd++; }
static void want(sh4_t *S, int64_t t) { if (t > S->need) S->need = t; }

static uint32_t RG(sh4_t *S, unsigned n) { want(S, S->rt_r[n]); return S->r[n]; }
static void WG(sh4_t *S, unsigned n, uint32_t v, int lat) { S->r[n] = v; pend(S, &S->rt_r[n], lat); }
static uint32_t RF(sh4_t *S, unsigned n) { const unsigned b = FRB(S); want(S, S->rt_f[b][n]); want(S, S->rt_fpscr); return S->fp[b][n]; }
static void WF(sh4_t *S, unsigned n, uint32_t v, int lat) { const unsigned b = FRB(S); S->fp[b][n] = v; pend(S, &S->rt_f[b][n], lat); }
static uint32_t RX(sh4_t *S, unsigned n) { const unsigned b = FRB(S) ^ 1u; want(S, S->rt_f[b][n]); want(S, S->rt_fpscr); return S->fp[b][n]; }
static void WX(sh4_t *S, unsigned n, uint32_t v, int lat) { const unsigned b = FRB(S) ^ 1u; S->fp[b][n] = v; pend(S, &S->rt_f[b][n], lat); }
static uint32_t RT(sh4_t *S) { want(S, S->rt_t); return S->t; }
static uint32_t RT_branch(sh4_t *S) { want(S, S->t_by_mt ? S->rt_t - 1 : S->rt_t); return S->t; }   /* an MT compare and its branch share a cycle */
static void WT(sh4_t *S, uint32_t v, int lat) { S->t = v & 1u; pend(S, &S->rt_t, lat); S->t_by_mt = S->grp == G_MT; }

static float f_of(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t b_of(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

/* FPSCR.DN = 1: a denormal operand is a zero of its sign, and a result under the normal range is one too */
static uint32_t dn_in(sh4_t *S, uint32_t b) {
    if (DN(S) && (b & 0x7f800000u) == 0u && (b & 0x007fffffu) != 0u) { S->st.denorm++; return b & 0x80000000u; }
    return b;
}
/* a NaN result of single-precision arithmetic is the SH-4's one quiet NaN (the manual's qnan(): H'7FBFFFFF), whatever the operands
   were: the host would hand on one operand's bits, and which one depends on the operand order the compiler chose */
static uint32_t dn_out(sh4_t *S, float f) { return f != f ? 0x7fbfffffu : dn_in(S, b_of(f)); }

static double d_of(uint32_t hi, uint32_t lo) { const uint64_t u = ((uint64_t)hi << 32) | lo; double d; memcpy(&d, &u, 8); return d; }
static void d_to(double d, uint32_t *hi, uint32_t *lo) { uint64_t u; memcpy(&u, &d, 8); *hi = (uint32_t)(u >> 32); *lo = (uint32_t)u; }
static double RD(sh4_t *S, unsigned n) { const uint32_t hi = RF(S, n & 14u), lo = RF(S, (n & 14u) + 1u); return d_of(hi, lo); }
static void WD(sh4_t *S, unsigned n, double d, int lat) { uint32_t hi, lo; d_to(d, &hi, &lo); WF(S, n & 14u, hi, lat); WF(S, (n & 14u) + 1u, lo, lat); }

/* ---- the vector unit ------------------------------------------------------------------------------------------------------------------ */

static uint32_t do_fipr(sh4_t *S, const uint32_t a[4], const uint32_t b[4]) {   /* a: FVm (the multiplicand), b: FVn */
#ifdef SH4RUN_HW_MODEL
    return sh4_fipr(a, b, (S->fpscr & 3u) == 1u);
#else
    double s = 0.0;
    int i;
    for (i = 0; i < 4; i++) s += (double)f_of(dn_in(S, a[i])) * (double)f_of(dn_in(S, b[i]));
    return dn_out(S, (float)s);
#endif
}

static uint32_t do_fsrra(sh4_t *S, uint32_t x) {
#ifdef SH4RUN_HW_MODEL
    return sh4_fsrra(x, (S->fpscr & 3u) == 1u);
#else
    return dn_out(S, 1.0f / sqrtf(f_of(dn_in(S, x))));
#endif
}

static void do_fsca(sh4_t *S, uint32_t fpul, uint32_t *sn, uint32_t *cs) {
#ifdef SH4RUN_HW_MODEL
    sh4_fsca(fpul, (S->fpscr & 3u) == 1u, sn, cs);
#else
    const double a = (double)(fpul & 0xffffu) * (6.283185307179586 / 65536.0);
    (void)S;
    *sn = b_of((float)sin(a)); *cs = b_of((float)cos(a));
#endif
}

int sh4_hw_model(void) {
#ifdef SH4RUN_HW_MODEL
    return 1;
#else
    return 0;
#endif
}

/* ---- timing ---------------------------------------------------------------------------------------------------------------------------- */

static int pairable(int first, int second) {
    if (first == G_CO || second == G_CO) return 0;
    if (first == G_MT || second == G_MT) return 1;
    return first != second;
}

/* Decide the cycle this instruction issues in, then stamp the ready time of everything it wrote. Returns the cycle. */
static int64_t issue(sh4_t *S) {
    int64_t e = (S->slot == 1 && pairable(S->prev_grp, S->grp)) ? S->cyc : S->cyc + 1;
    int k;
    if (S->need > e) e = S->need;
    if (S->resume > e) e = S->resume;
    if (S->grp == G_FE && S->fe_free > e) e = S->fe_free;
    if (S->div_hold && S->div_free > e) e = S->div_free;
    if (e == S->cyc) S->slot = 2;
    else { S->cyc = e; S->slot = 1; }
    if (S->grp == G_CO) { S->cyc += S->rate - 1; S->slot = 2; }
    S->prev_grp = S->grp;
    for (k = 0; k < S->npd; k++) *S->pd[k].p = e + S->pd[k].lat;
    if (S->grp == G_FE) S->fe_free = e + (S->fe_hold ? S->fe_hold : 1);
    if (S->div_hold) S->div_free = e + S->div_hold;
    S->npd = 0; S->need = 0; S->fe_hold = 0; S->div_hold = 0;
    return e;
}

/* ---- the executor ---------------------------------------------------------------------------------------------------------------------- */

static int32_t sx8(uint32_t v) { return (int32_t)(int8_t)(v & 0xffu); }
static int32_t sx12(uint32_t v) { return (int32_t)((v & 0x800u) ? (v | 0xfffff000u) : (v & 0xfffu)); }

static void fp_single_only(sh4_t *S, const char *name) { if (PR(S)) fault(S, "%s with FPSCR.PR = 1 is undefined", name); }

/* One instruction at S->pc. *target: where a taken branch goes; returns 0: no branch, 1: taken now, 2: taken after the delay slot. */
static int exec(sh4_t *S, uint16_t op, uint32_t *target, int *branch_lat) {
    const unsigned id = lut[op] ? opdefs[lut[op] - 1].id : OP_BAD;
    const unsigned n = (op >> 8) & 15u, m = (op >> 4) & 15u, d4 = op & 15u, d8 = op & 255u;
    const uint32_t pc = S->pc;
    uint32_t a, b, v;
    int jump = 0;

    S->grp = lut[op] ? opdefs[lut[op] - 1].grp : G_CO;
    S->rate = lut[op] ? opdefs[lut[op] - 1].rate : 1;
    *branch_lat = 2;

    switch (id) {
    /* -- moves -- */
    case OP_MOVI:   WG(S, n, (uint32_t)sx8(d8), 0); break;
    case OP_MOVWI:  WG(S, n, (uint32_t)(int32_t)(int16_t)rd16(S, pc + 4u + d8 * 2u), 2); break;
    case OP_MOVLI:  WG(S, n, rd32(S, (pc & ~3u) + 4u + d8 * 4u), 2); break;
    case OP_MOV:    WG(S, n, RG(S, m), 0); break;
    case OP_MOVBS:  wr8(S, RG(S, n), RG(S, m)); break;
    case OP_MOVWS:  wr16(S, RG(S, n), RG(S, m)); break;
    case OP_MOVLS:  wr32(S, RG(S, n), RG(S, m)); break;
    case OP_MOVBL:  WG(S, n, (uint32_t)(int32_t)(int8_t)rd8(S, RG(S, m)), 2); break;
    case OP_MOVWL:  WG(S, n, (uint32_t)(int32_t)(int16_t)rd16(S, RG(S, m)), 2); break;
    case OP_MOVLL:  WG(S, n, rd32(S, RG(S, m)), 2); break;
    case OP_MOVBM:  v = RG(S, m); a = RG(S, n) - 1u; wr8(S, a, v); WG(S, n, a, 1); break;
    case OP_MOVWM:  v = RG(S, m); a = RG(S, n) - 2u; wr16(S, a, v); WG(S, n, a, 1); break;
    case OP_MOVLM:  v = RG(S, m); a = RG(S, n) - 4u; wr32(S, a, v); WG(S, n, a, 1); break;
    case OP_MOVBP:  a = RG(S, m); v = (uint32_t)(int32_t)(int8_t)rd8(S, a); if (n != m) WG(S, m, a + 1u, 1); WG(S, n, v, 2); break;
    case OP_MOVWP:  a = RG(S, m); v = (uint32_t)(int32_t)(int16_t)rd16(S, a); if (n != m) WG(S, m, a + 2u, 1); WG(S, n, v, 2); break;
    case OP_MOVLP:  a = RG(S, m); v = rd32(S, a); if (n != m) WG(S, m, a + 4u, 1); WG(S, n, v, 2); break;
    case OP_MOVBS4: wr8(S, RG(S, m) + d4, RG(S, 0)); break;                                  /* the register field of these two is bits 7-4 */
    case OP_MOVWS4: wr16(S, RG(S, m) + d4 * 2u, RG(S, 0)); break;
    case OP_MOVLS4: wr32(S, RG(S, n) + d4 * 4u, RG(S, m)); break;
    case OP_MOVBL4: WG(S, 0, (uint32_t)(int32_t)(int8_t)rd8(S, RG(S, m) + d4), 2); break;
    case OP_MOVWL4: WG(S, 0, (uint32_t)(int32_t)(int16_t)rd16(S, RG(S, m) + d4 * 2u), 2); break;
    case OP_MOVLL4: WG(S, n, rd32(S, RG(S, m) + d4 * 4u), 2); break;
    case OP_MOVBS0: wr8(S, RG(S, n) + RG(S, 0), RG(S, m)); break;
    case OP_MOVWS0: wr16(S, RG(S, n) + RG(S, 0), RG(S, m)); break;
    case OP_MOVLS0: wr32(S, RG(S, n) + RG(S, 0), RG(S, m)); break;
    case OP_MOVBL0: WG(S, n, (uint32_t)(int32_t)(int8_t)rd8(S, RG(S, m) + RG(S, 0)), 2); break;
    case OP_MOVWL0: WG(S, n, (uint32_t)(int32_t)(int16_t)rd16(S, RG(S, m) + RG(S, 0)), 2); break;
    case OP_MOVLL0: WG(S, n, rd32(S, RG(S, m) + RG(S, 0)), 2); break;
    case OP_MOVBSG: wr8(S, S->gbr + d8, RG(S, 0)); break;
    case OP_MOVWSG: wr16(S, S->gbr + d8 * 2u, RG(S, 0)); break;
    case OP_MOVLSG: wr32(S, S->gbr + d8 * 4u, RG(S, 0)); break;
    case OP_MOVBLG: WG(S, 0, (uint32_t)(int32_t)(int8_t)rd8(S, S->gbr + d8), 2); break;
    case OP_MOVWLG: WG(S, 0, (uint32_t)(int32_t)(int16_t)rd16(S, S->gbr + d8 * 2u), 2); break;
    case OP_MOVLLG: WG(S, 0, rd32(S, S->gbr + d8 * 4u), 2); break;
    case OP_MOVA:   WG(S, 0, (pc & ~3u) + 4u + d8 * 4u, 1); break;
    case OP_MOVT:   WG(S, n, RT(S), 1); break;
    case OP_SWAPB:  v = RG(S, m); WG(S, n, (v & 0xffff0000u) | ((v & 0xffu) << 8) | ((v >> 8) & 0xffu), 1); break;
    case OP_SWAPW:  v = RG(S, m); WG(S, n, (v << 16) | (v >> 16), 1); break;
    case OP_XTRCT:  WG(S, n, (RG(S, m) << 16) | (RG(S, n) >> 16), 1); break;
    case OP_PREF:
        a = RG(S, n); S->st.pref++;
        if (a >= 0xe0000000u && a < 0xe4000000u) fault(S, "pref on a store queue address %08x: store queues are not modelled", (unsigned)a);
        break;
    case OP_OCB:    (void)RG(S, n); break;
    case OP_MOVCAL: {
        uint8_t *p;
        a = RG(S, n); v = RG(S, 0); S->st.movca++;
        p = mem(S, a, 4, "write (movca.l)");
        if (p) {
            if (S->poison) memset(S->ram + ((a & ~31u) - SH4_RAM_BASE), 0xa5, 32);   /* a line allocated without a read holds nothing */
            memcpy(p, &v, 4);
        }
        S->st.stores++;
        break;
    }

    /* -- arithmetic -- */
    case OP_ADD:    WG(S, n, RG(S, n) + RG(S, m), 1); break;
    case OP_ADDI:   WG(S, n, RG(S, n) + (uint32_t)sx8(d8), 1); break;
    case OP_ADDC:   { const uint64_t s = (uint64_t)RG(S, n) + RG(S, m) + RT(S); WG(S, n, (uint32_t)s, 1); WT(S, (uint32_t)(s >> 32), 1); break; }
    case OP_ADDV:   { const int64_t s = (int64_t)(int32_t)RG(S, n) + (int32_t)RG(S, m); WG(S, n, (uint32_t)s, 1); WT(S, s != (int32_t)s, 1); break; }
    case OP_CMPIM:  WT(S, RG(S, 0) == (uint32_t)sx8(d8), 1); break;
    case OP_CMPEQ:  WT(S, RG(S, n) == RG(S, m), 1); break;
    case OP_CMPHS:  WT(S, RG(S, n) >= RG(S, m), 1); break;
    case OP_CMPGE:  WT(S, (int32_t)RG(S, n) >= (int32_t)RG(S, m), 1); break;
    case OP_CMPHI:  WT(S, RG(S, n) > RG(S, m), 1); break;
    case OP_CMPGT:  WT(S, (int32_t)RG(S, n) > (int32_t)RG(S, m), 1); break;
    case OP_CMPPL:  WT(S, (int32_t)RG(S, n) > 0, 1); break;
    case OP_CMPPZ:  WT(S, (int32_t)RG(S, n) >= 0, 1); break;
    case OP_CMPSTR: v = RG(S, n) ^ RG(S, m); WT(S, !(v & 0xff000000u) || !(v & 0x00ff0000u) || !(v & 0x0000ff00u) || !(v & 0x000000ffu), 1); break;
    case OP_DIV0S:  S->q = RG(S, n) >> 31; S->m = RG(S, m) >> 31; WT(S, S->q ^ S->m, 1); break;
    case OP_DIV0U:  S->q = 0; S->m = 0; WT(S, 0, 1); break;
    case OP_DIV1: {
        const uint32_t old_q = S->q, rm = RG(S, m);
        uint32_t rn = RG(S, n), tmp0, tmp1;
        S->q = rn >> 31;
        rn = (rn << 1) | RT(S);
        tmp0 = rn;
        /* the manual's four cases (old Q, M), folded: subtract when they agree, add when they differ; the new Q is the borrow or
           carry, inverted when the bit shifted out was 1, and inverted again when M is 1 */
        if (old_q == S->m) { rn -= rm; tmp1 = rn > tmp0; }
        else               { rn += rm; tmp1 = rn < tmp0; }
        S->q = (S->q ? (tmp1 == 0u) : tmp1) ^ S->m;
        WG(S, n, rn, 1);
        WT(S, S->q == S->m, 1);
        break;
    }
    case OP_DMULS:  { const int64_t p = (int64_t)(int32_t)RG(S, n) * (int32_t)RG(S, m); S->mach = (uint32_t)((uint64_t)p >> 32); S->macl = (uint32_t)p; pend(S, &S->rt_mac, 4); break; }
    case OP_DMULU:  { const uint64_t p = (uint64_t)RG(S, n) * RG(S, m); S->mach = (uint32_t)(p >> 32); S->macl = (uint32_t)p; pend(S, &S->rt_mac, 4); break; }
    case OP_DT:     v = RG(S, n) - 1u; WG(S, n, v, 1); WT(S, v == 0u, 1); break;
    case OP_EXTSB:  WG(S, n, (uint32_t)(int32_t)(int8_t)RG(S, m), 1); break;
    case OP_EXTSW:  WG(S, n, (uint32_t)(int32_t)(int16_t)RG(S, m), 1); break;
    case OP_EXTUB:  WG(S, n, RG(S, m) & 0xffu, 1); break;
    case OP_EXTUW:  WG(S, n, RG(S, m) & 0xffffu, 1); break;
    case OP_MULL:   S->macl = RG(S, n) * RG(S, m); pend(S, &S->rt_mac, 4); break;
    case OP_MULS:   S->macl = (uint32_t)((int32_t)(int16_t)RG(S, n) * (int32_t)(int16_t)RG(S, m)); pend(S, &S->rt_mac, 4); break;
    case OP_MULU:   S->macl = (RG(S, n) & 0xffffu) * (RG(S, m) & 0xffffu); pend(S, &S->rt_mac, 4); break;
    case OP_NEG:    WG(S, n, 0u - RG(S, m), 1); break;
    case OP_NEGC:   { const uint32_t rm = RG(S, m), tt = RT(S); WG(S, n, 0u - rm - tt, 1); WT(S, rm != 0u || tt != 0u, 1); break; }
    case OP_SUB:    WG(S, n, RG(S, n) - RG(S, m), 1); break;
    case OP_SUBC:   { const uint32_t rn = RG(S, n), rm = RG(S, m), tt = RT(S); WG(S, n, rn - rm - tt, 1); WT(S, (uint64_t)rn < (uint64_t)rm + tt, 1); break; }
    case OP_SUBV:   { const int64_t s = (int64_t)(int32_t)RG(S, n) - (int32_t)RG(S, m); WG(S, n, (uint32_t)s, 1); WT(S, s != (int32_t)s, 1); break; }

    /* -- logic -- */
    case OP_AND:    WG(S, n, RG(S, n) & RG(S, m), 1); break;
    case OP_ANDI:   WG(S, 0, RG(S, 0) & d8, 1); break;
    case OP_NOT:    WG(S, n, ~RG(S, m), 1); break;
    case OP_OR:     WG(S, n, RG(S, n) | RG(S, m), 1); break;
    case OP_ORI:    WG(S, 0, RG(S, 0) | d8, 1); break;
    case OP_TST:    WT(S, (RG(S, n) & RG(S, m)) == 0u, 1); break;
    case OP_TSTI:   WT(S, (RG(S, 0) & d8) == 0u, 1); break;
    case OP_XOR:    WG(S, n, RG(S, n) ^ RG(S, m), 1); break;
    case OP_XORI:   WG(S, 0, RG(S, 0) ^ d8, 1); break;

    /* -- shifts -- */
    case OP_ROTL:   v = RG(S, n); WT(S, v >> 31, 1); WG(S, n, (v << 1) | (v >> 31), 1); break;
    case OP_ROTR:   v = RG(S, n); WT(S, v & 1u, 1); WG(S, n, (v >> 1) | (v << 31), 1); break;
    case OP_ROTCL:  v = RG(S, n); a = RT(S); WT(S, v >> 31, 1); WG(S, n, (v << 1) | a, 1); break;
    case OP_ROTCR:  v = RG(S, n); a = RT(S); WT(S, v & 1u, 1); WG(S, n, (v >> 1) | (a << 31), 1); break;
    case OP_SHAD:
        v = RG(S, n); b = RG(S, m);
        if (!(b & 0x80000000u)) v <<= (b & 31u);
        else if (!(b & 31u)) v = (uint32_t)((int32_t)v < 0 ? -1 : 0);
        else v = (uint32_t)((int32_t)v >> (((~b) & 31u) + 1u));
        WG(S, n, v, 1);
        break;
    case OP_SHLD:
        v = RG(S, n); b = RG(S, m);
        if (!(b & 0x80000000u)) v <<= (b & 31u);
        else if (!(b & 31u)) v = 0u;
        else v >>= (((~b) & 31u) + 1u);
        WG(S, n, v, 1);
        break;
    case OP_SHAL: case OP_SHLL: v = RG(S, n); WT(S, v >> 31, 1); WG(S, n, v << 1, 1); break;
    case OP_SHAR:   v = RG(S, n); WT(S, v & 1u, 1); WG(S, n, (uint32_t)((int32_t)v >> 1), 1); break;
    case OP_SHLR:   v = RG(S, n); WT(S, v & 1u, 1); WG(S, n, v >> 1, 1); break;
    case OP_SHLL2:  WG(S, n, RG(S, n) << 2, 1); break;
    case OP_SHLL8:  WG(S, n, RG(S, n) << 8, 1); break;
    case OP_SHLL16: WG(S, n, RG(S, n) << 16, 1); break;
    case OP_SHLR2:  WG(S, n, RG(S, n) >> 2, 1); break;
    case OP_SHLR8:  WG(S, n, RG(S, n) >> 8, 1); break;
    case OP_SHLR16: WG(S, n, RG(S, n) >> 16, 1); break;

    /* -- branches -- */
    case OP_BF:     if (!RT_branch(S)) { *target = pc + 4u + (uint32_t)(sx8(d8) * 2); jump = 1; } break;
    case OP_BT:     if (RT_branch(S))  { *target = pc + 4u + (uint32_t)(sx8(d8) * 2); jump = 1; } break;
    case OP_BFS:    if (!RT_branch(S)) { *target = pc + 4u + (uint32_t)(sx8(d8) * 2); jump = 2; } break;
    case OP_BTS:    if (RT_branch(S))  { *target = pc + 4u + (uint32_t)(sx8(d8) * 2); jump = 2; } break;
    case OP_BRA:    *target = pc + 4u + (uint32_t)(sx12(op) * 2); jump = 2; break;
    case OP_BSR:    S->pr = pc + 4u; pend(S, &S->rt_pr, 2); *target = pc + 4u + (uint32_t)(sx12(op) * 2); jump = 2; break;
    case OP_BRAF:   *target = pc + 4u + RG(S, n); jump = 2; *branch_lat = 3; break;
    case OP_BSRF:   S->pr = pc + 4u; pend(S, &S->rt_pr, 2); *target = pc + 4u + RG(S, n); jump = 2; *branch_lat = 3; break;
    case OP_JMP:    *target = RG(S, n); jump = 2; *branch_lat = 3; break;
    case OP_JSR:    S->pr = pc + 4u; pend(S, &S->rt_pr, 2); *target = RG(S, n); jump = 2; *branch_lat = 3; break;
    case OP_RTS:    want(S, S->rt_pr); *target = S->pr; jump = 2; *branch_lat = 3; break;

    /* -- system -- */
    case OP_CLRMAC: S->mach = 0; S->macl = 0; pend(S, &S->rt_mac, 3); break;
    case OP_CLRS:   S->s = 0; break;
    case OP_SETS:   S->s = 1; break;
    case OP_CLRT:   WT(S, 0, 1); break;
    case OP_SETT:   WT(S, 1, 1); break;
    case OP_NOP:    break;
    case OP_LDCGBR: S->gbr = RG(S, n); break;
    case OP_STCGBR: WG(S, n, S->gbr, 2); break;
    case OP_LDSMACH: S->mach = RG(S, n); pend(S, &S->rt_mac, 3); break;
    case OP_LDSMACL: S->macl = RG(S, n); pend(S, &S->rt_mac, 3); break;
    case OP_LDSPR:  S->pr = RG(S, n); pend(S, &S->rt_pr, 3); break;
    case OP_LDSMMACH: a = RG(S, n); S->mach = rd32(S, a); WG(S, n, a + 4u, 1); pend(S, &S->rt_mac, 3); break;
    case OP_LDSMMACL: a = RG(S, n); S->macl = rd32(S, a); WG(S, n, a + 4u, 1); pend(S, &S->rt_mac, 3); break;
    case OP_LDSMPR: a = RG(S, n); S->pr = rd32(S, a); WG(S, n, a + 4u, 1); pend(S, &S->rt_pr, 3); break;
    case OP_STSMACH: want(S, S->rt_mac); WG(S, n, S->mach, 3); break;
    case OP_STSMACL: want(S, S->rt_mac); WG(S, n, S->macl, 3); break;
    case OP_STSPR:  want(S, S->rt_pr); WG(S, n, S->pr, 2); break;
    case OP_STSMMACH: want(S, S->rt_mac); a = RG(S, n) - 4u; wr32(S, a, S->mach); WG(S, n, a, 1); break;
    case OP_STSMMACL: want(S, S->rt_mac); a = RG(S, n) - 4u; wr32(S, a, S->macl); WG(S, n, a, 1); break;
    case OP_STSMPR: want(S, S->rt_pr); a = RG(S, n) - 4u; wr32(S, a, S->pr); WG(S, n, a, 1); break;
    case OP_LDSFPUL: S->fpul = RG(S, n); pend(S, &S->rt_fpul, 1); pend(S, &S->rt_fpul_sts, 1); break;
    case OP_LDSFPSCR: S->fpscr = RG(S, n) & 0x003fffffu; pend(S, &S->rt_fpscr, 4); break;
    case OP_LDSMFPUL: a = RG(S, n); S->fpul = rd32(S, a); WG(S, n, a + 4u, 1); pend(S, &S->rt_fpul, 2); pend(S, &S->rt_fpul_sts, 2); break;
    case OP_LDSMFPSCR: a = RG(S, n); S->fpscr = rd32(S, a) & 0x003fffffu; WG(S, n, a + 4u, 1); pend(S, &S->rt_fpscr, 4); break;
    case OP_STSFPUL: want(S, S->rt_fpul_sts); WG(S, n, S->fpul, 3); break;
    case OP_STSFPSCR: want(S, S->rt_fpscr); WG(S, n, S->fpscr, 3); break;
    case OP_STSMFPUL: want(S, S->rt_fpul_sts); a = RG(S, n) - 4u; wr32(S, a, S->fpul); WG(S, n, a, 1); break;
    case OP_STSMFPSCR: want(S, S->rt_fpscr); a = RG(S, n) - 4u; wr32(S, a, S->fpscr); WG(S, n, a, 1); break;

    /* -- FPU -- */
    case OP_FSCHG:  fp_single_only(S, "fschg"); S->fpscr ^= 1u << 20; pend(S, &S->rt_fpscr, 1); S->st.mode++; break;
    case OP_FRCHG:  fp_single_only(S, "frchg"); S->fpscr ^= 1u << 21; pend(S, &S->rt_fpscr, 1); S->st.mode++; break;
    case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV:
        if (PR(S)) {
            const double x = RD(S, n), y = RD(S, m);
            WD(S, n, id == OP_FADD ? x + y : id == OP_FSUB ? x - y : id == OP_FMUL ? x * y : x / y, id == OP_FDIV ? 25 : 8);
            S->fe_hold = id == OP_FDIV ? 23 : 6;
        } else {
            const float x = f_of(dn_in(S, RF(S, n))), y = f_of(dn_in(S, RF(S, m)));
            if (id == OP_FDIV) { WF(S, n, dn_out(S, x / y), 12); S->div_hold = 10; S->st.fdiv++; }
            else if (id == OP_FADD) { WF(S, n, dn_out(S, x + y), 3); S->st.fadd++; }
            else if (id == OP_FSUB) { WF(S, n, dn_out(S, x - y), 3); S->st.fsub++; }
            else { WF(S, n, dn_out(S, x * y), 3); S->st.fmul++; }
        }
        break;
    case OP_FCMPEQ: case OP_FCMPGT:
        S->st.fcmp++;
        if (PR(S)) { const double x = RD(S, n), y = RD(S, m); WT(S, id == OP_FCMPEQ ? x == y : x > y, 3); S->grp = G_CO; S->rate = 2; }
        else { const float x = f_of(dn_in(S, RF(S, n))), y = f_of(dn_in(S, RF(S, m))); WT(S, id == OP_FCMPEQ ? x == y : x > y, 2); }
        break;
    case OP_FMOV:
        if (SZ(S)) {   /* a pair: bit 0 of a field picks the other bank (XD) */
            const uint32_t lo = (m & 1u) ? RX(S, m & 14u) : RF(S, m & 14u), hi = (m & 1u) ? RX(S, (m & 14u) + 1u) : RF(S, (m & 14u) + 1u);
            if (n & 1u) { WX(S, n & 14u, lo, 0); WX(S, (n & 14u) + 1u, hi, 0); }
            else        { WF(S, n & 14u, lo, 0); WF(S, (n & 14u) + 1u, hi, 0); }
        } else WF(S, n, RF(S, m), 0);
        break;
    case OP_FMOVL: case OP_FMOVP: case OP_FMOVIL:
        a = RG(S, m);
        if (id == OP_FMOVIL) a += RG(S, 0);
        want(S, S->rt_fpscr);
        if (SZ(S)) {
            uint32_t lo, hi;
            rd64(S, a, &lo, &hi);
            if (n & 1u) { WX(S, n & 14u, lo, 2); WX(S, (n & 14u) + 1u, hi, 2); }
            else        { WF(S, n & 14u, lo, 2); WF(S, (n & 14u) + 1u, hi, 2); }
            if (id == OP_FMOVP) WG(S, m, a + 8u, 1);
        } else {
            WF(S, n, rd32(S, a), 2);
            if (id == OP_FMOVP) WG(S, m, a + 4u, 1);
        }
        break;
    case OP_FMOVS: case OP_FMOVM: case OP_FMOVIS:
        a = RG(S, n);
        if (id == OP_FMOVIS) a += RG(S, 0);
        if (SZ(S)) {
            const uint32_t lo = (m & 1u) ? RX(S, m & 14u) : RF(S, m & 14u), hi = (m & 1u) ? RX(S, (m & 14u) + 1u) : RF(S, (m & 14u) + 1u);
            if (id == OP_FMOVM) { a -= 8u; WG(S, n, a, 1); }
            wr64(S, a, lo, hi);
        } else {
            v = RF(S, m);
            if (id == OP_FMOVM) { a -= 4u; WG(S, n, a, 1); }
            wr32(S, a, v);
        }
        break;
    case OP_FSTS:   want(S, S->rt_fpul); WF(S, n, S->fpul, 0); break;
    case OP_FLDS:   S->fpul = RF(S, n); pend(S, &S->rt_fpul, 0); pend(S, &S->rt_fpul_sts, 0); break;
    case OP_FLOAT:
        want(S, S->rt_fpul); want(S, S->rt_fpscr); S->st.cvt++;
        if (PR(S)) { WD(S, n, (double)(int32_t)S->fpul, 4); S->fe_hold = 2; }
        else WF(S, n, b_of((float)(int32_t)S->fpul), 3);
        break;
    case OP_FTRC: {
        const double x = PR(S) ? RD(S, n) : (double)f_of(dn_in(S, RF(S, n)));
        S->st.cvt++;
        if (x != x) S->fpul = 0x80000000u;                 /* a NaN: the manual's invalid result */
        else if (x >= 2147483648.0) S->fpul = 0x7fffffffu;
        else if (x <= -2147483649.0) S->fpul = 0x80000000u;
        else S->fpul = (uint32_t)(int32_t)x;
        pend(S, &S->rt_fpul, PR(S) ? 4 : 3);
        pend(S, &S->rt_fpul_sts, PR(S) ? 4 : 1);   /* the truncated integer reaches STS FPUL after one cycle (GCC sh4.md: bypass 1) */
        if (PR(S)) S->fe_hold = 2;
        break;
    }
    case OP_FNEG:   WF(S, PR(S) ? (n & 14u) : n, RF(S, PR(S) ? (n & 14u) : n) ^ 0x80000000u, 0); break;
    case OP_FABS:   WF(S, PR(S) ? (n & 14u) : n, RF(S, PR(S) ? (n & 14u) : n) & 0x7fffffffu, 0); break;
    case OP_FSQRT:
        S->st.fsqrt++;
        if (PR(S)) { WD(S, n, sqrt(RD(S, n)), 24); S->fe_hold = 22; }
        else { WF(S, n, dn_out(S, sqrtf(f_of(dn_in(S, RF(S, n))))), 11); S->div_hold = 9; }
        break;
    case OP_FSRRA:  fp_single_only(S, "fsrra"); S->st.fsrra++; WF(S, n, do_fsrra(S, RF(S, n)), 5); S->fe_hold = 3; break;
    case OP_FLDI0:  fp_single_only(S, "fldi0"); want(S, S->rt_fpscr); WF(S, n, 0x00000000u, 0); break;
    case OP_FLDI1:  fp_single_only(S, "fldi1"); want(S, S->rt_fpscr); WF(S, n, 0x3f800000u, 0); break;
    case OP_FCNVSD: if (!PR(S)) fault(S, "fcnvsd with FPSCR.PR = 0 is undefined"); want(S, S->rt_fpul); WD(S, n, (double)f_of(S->fpul), 4); S->fe_hold = 2; break;
    case OP_FCNVDS: if (!PR(S)) fault(S, "fcnvds with FPSCR.PR = 0 is undefined"); S->fpul = b_of((float)RD(S, n)); pend(S, &S->rt_fpul, 4); pend(S, &S->rt_fpul_sts, 4); S->fe_hold = 2; break;
    case OP_FIPR: {
        const unsigned vn = (n & 12u), vm = (n & 3u) << 2;
        uint32_t x[4], y[4];
        int k;
        fp_single_only(S, "fipr"); S->st.fipr++;
        for (k = 0; k < 4; k++) { x[k] = RF(S, vm + (unsigned)k); y[k] = RF(S, vn + (unsigned)k); }
        WF(S, vn + 3u, do_fipr(S, x, y), 4);
        break;
    }
    case OP_FTRV: {
        const unsigned vn = (n & 12u);
        uint32_t x[4], out[4], row[4];
        int i, j;
        fp_single_only(S, "ftrv"); S->st.ftrv++;
        for (j = 0; j < 4; j++) x[j] = RF(S, vn + (unsigned)j);
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) row[j] = RX(S, (unsigned)(4 * j + i));   /* XF is column major: row i is XF[i], XF[4 + i], ... */
            out[i] = do_fipr(S, x, row);                                       /* the vector is the multiplicand of each of the four inner products */
        }
        WF(S, vn, out[0], 5); WF(S, vn + 1u, out[1], 5); WF(S, vn + 2u, out[2], 6); WF(S, vn + 3u, out[3], 7);
        S->fe_hold = 4;
        break;
    }
    case OP_FSCA: {
        uint32_t sn, cs;
        fp_single_only(S, "fsca"); S->st.fsca++;
        want(S, S->rt_fpul);
        do_fsca(S, S->fpul, &sn, &cs);
        WF(S, n & 14u, sn, 6); WF(S, (n & 14u) + 1u, cs, 7);
        S->fe_hold = 4;
        break;
    }
    case OP_FMAC: {
        const float x = f_of(dn_in(S, RF(S, 0))), y = f_of(dn_in(S, RF(S, m))), z = f_of(dn_in(S, RF(S, n)));
        fp_single_only(S, "fmac"); S->st.fmac++;
        WF(S, n, dn_out(S, fmaf(x, y, z)), 3);
        break;
    }

    default:
        fault(S, "unknown instruction %04x (%s)", (unsigned)op, sh4_mnemonic(op));
        break;
    }
    return jump;
}

static int is_branch(uint16_t op) {
    const unsigned id = lut[op] ? opdefs[lut[op] - 1].id : OP_BAD;
    return id >= OP_BF && id <= OP_RTS;
}

static void step(sh4_t *S) {
    uint32_t target = 0;
    int lat = 2, jump;
    int64_t e;
    uint16_t op;
    const uint8_t *p = mem(S, S->pc, 2, "fetch");
    if (!p) return;
    memcpy(&op, p, 2);
    jump = exec(S, op, &target, &lat);
    e = issue(S);
    S->st.instrs++;
    if (S->trace) fprintf(stderr, "%8lld %08x %04x %s\n", (long long)e, (unsigned)S->pc, (unsigned)op, sh4_mnemonic(op));
    if (S->halt) return;
    if (jump == 2) {   /* the delay slot runs first */
        uint16_t slot_op;
        uint32_t slot_target = 0;
        int slot_lat = 2;
        const uint32_t branch_pc = S->pc;
        S->pc = branch_pc + 2u;
        p = mem(S, S->pc, 2, "fetch");
        if (!p) return;
        memcpy(&slot_op, p, 2);
        if (is_branch(slot_op)) { fault(S, "a branch in a delay slot"); return; }
        (void)exec(S, slot_op, &slot_target, &slot_lat);
        (void)issue(S);
        S->st.instrs++;
        if (S->trace) fprintf(stderr, "%8s %08x %04x %s (slot)\n", "", (unsigned)S->pc, (unsigned)slot_op, sh4_mnemonic(slot_op));
        if (S->halt) return;
    }
    if (jump) {
        S->resume = e + lat;
        S->st.taken++;
        S->pc = target;
    } else S->pc += 2u;
    if (S->st.instrs > S->limit) fault(S, "no return after %llu instructions", (unsigned long long)S->limit);
}

/* ---- the public half ------------------------------------------------------------------------------------------------------------------- */

sh4_t *sh4_new(void) {
    sh4_t *S = (sh4_t *)calloc(1, sizeof *S);
    if (!S) return NULL;
    lut_build();
    S->ram = (uint8_t *)calloc(1, SH4_RAM_SIZE);
    S->syms = (sym_t *)calloc(MAX_SYMS, sizeof(sym_t));
    if (!S->ram || !S->syms) { sh4_free(S); return NULL; }
    S->fpscr = FPSCR_INIT;
    S->heap = HEAP_BASE;
    S->limit = MAX_INSTRS;
    S->poison = 1;
    S->seed = 0x2545f491u;
    return S;
}

void sh4_free(sh4_t *S) {
    if (!S) return;
    free(S->ram); free(S->syms); free(S);
}

static uint32_t le32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint32_t le16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }

int sh4_load_elf(sh4_t *S, const char *path) {
    FILE *f = fopen(path, "rb");
    uint8_t *img;
    long size;
    uint32_t shoff, shnum, shentsize, i;
    if (!f) { snprintf(S->err, sizeof S->err, "cannot open %s", path); return -1; }
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    img = (uint8_t *)malloc((size_t)size + 1u);
    if (!img || fread(img, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(img); snprintf(S->err, sizeof S->err, "cannot read %s", path); return -1; }
    fclose(f);
    if (size < 52 || memcmp(img, "\177ELF", 4) != 0 || img[4] != 1 || img[5] != 1 || le16(img + 18) != 42u) {
        free(img); snprintf(S->err, sizeof S->err, "%s is not a little-endian 32-bit SH ELF", path); return -1;
    }
    shoff = le32(img + 32); shentsize = le16(img + 46); shnum = le16(img + 48);
    S->nsyms = 0; S->nxsec = 0;
    for (i = 0; i < shnum; i++) {
        const uint8_t *sh = img + shoff + i * shentsize;
        const uint32_t type = le32(sh + 4), flags = le32(sh + 8), addr = le32(sh + 12), off = le32(sh + 16), sz = le32(sh + 20), link = le32(sh + 24);
        if ((flags & 2u) && sz && (addr < SH4_RAM_BASE || addr >= SH4_RAM_BASE + SH4_RAM_SIZE))
            continue;   /* not in the RAM (the default linker script's .stack marker): left out, a use of it stops the run as any access outside the RAM */
        if ((flags & 2u) && sz) {   /* SHF_ALLOC */
            if (addr - SH4_RAM_BASE > SH4_RAM_SIZE - sz || addr + sz > HEAP_BASE) {
                free(img); snprintf(S->err, sizeof S->err, "section at %08x (+%u) does not fit under the test heap", (unsigned)addr, (unsigned)sz); return -1;
            }
            if (type == 8u) memset(S->ram + (addr - SH4_RAM_BASE), 0, sz);   /* SHT_NOBITS */
            else memcpy(S->ram + (addr - SH4_RAM_BASE), img + off, sz);
            if ((flags & 4u) && S->nxsec < MAX_XSECS) { S->xsec[S->nxsec].addr = addr; S->xsec[S->nxsec].size = sz; S->nxsec++; }
        }
        if (type == 2u) {   /* SHT_SYMTAB */
            const uint8_t *str = img + le32(img + shoff + link * shentsize + 16);
            uint32_t k;
            for (k = 0; k + 16u <= sz; k += 16u) {
                const uint8_t *e = img + off + k;
                const char *name = (const char *)str + le32(e);
                const unsigned kind = e[12] & 15u;
                if ((kind == 1u || kind == 2u || kind == 0u) && le16(e + 14) != 0u && name[0] && S->nsyms < MAX_SYMS) {
                    S->syms[S->nsyms].addr = le32(e + 4);
                    snprintf(S->syms[S->nsyms].name, sizeof S->syms[S->nsyms].name, "%s", name);
                    S->nsyms++;
                }
            }
        }
    }
    free(img);
    return 0;
}

uint32_t sh4_sym(const sh4_t *S, const char *name) {
    int k;
    for (k = 0; k < S->nsyms; k++)
        if (!strcmp(S->syms[k].name, name)) return S->syms[k].addr;
    return 0u;
}

uint32_t sh4_alloc(sh4_t *S, uint32_t bytes, uint32_t align) {
    uint32_t a;
    if (!align) align = 4u;
    a = (S->heap + align - 1u) & ~(align - 1u);
    if (a < S->heap || a > HEAP_END || bytes > HEAP_END - a) return 0u;
    S->heap = a + bytes;
    return a;
}

void sh4_alloc_reset(sh4_t *S) { S->heap = HEAP_BASE; }

void *sh4_ptr(sh4_t *S, uint32_t addr, uint32_t bytes) {
    if (addr < SH4_RAM_BASE || addr - SH4_RAM_BASE > SH4_RAM_SIZE - bytes) return NULL;
    return S->ram + (addr - SH4_RAM_BASE);
}

void sh4_float_args(sh4_t *S, const float *args, int count) {
    int k;
    S->nfargs = count > 8 ? 8 : count;
    for (k = 0; k < S->nfargs; k++) S->fargs[k] = args[k];
}

int sh4_call(sh4_t *S, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t keep_r[16], keep_f[4], keep_gbr, sp;
    const uint32_t mode = FPSCR_INIT & FPSCR_MODE;
    const int nan_run = (int)(S->calls & 1u);   /* every other call starts with NaNs in the scratch float registers */
    int k;

    S->fpscr = FPSCR_INIT;   /* every call starts as KallistiOS starts a thread (a call that failed half way may have left its mode bits) */
    S->calls++;
    S->halt = 0; S->err[0] = 0;
    memset(&S->st, 0, sizeof S->st);
    if (!fn) { snprintf(S->err, sizeof S->err, "call of a null function (symbol not found?)"); return -1; }

    for (k = 0; k < 16; k++) S->r[k] = rng(S);
    S->r[4] = a0; S->r[5] = a1; S->r[6] = a2; S->r[7] = a3;
    sp = STACK_TOP - ((S->calls & 2u) ? 4u : 0u);          /* 4-aligned always, 8-aligned only half the time: the ABI promises 4 */
    S->r[15] = sp;
    for (k = 0; k < 16; k++) {
        const uint32_t g = rng(S);
        S->fp[FRB(S)][k] = (k >= 12) ? g : nan_run ? (0x7fc00000u | (g & 0x003fffffu)) : (0x3f000000u + (g & 0x00ffffffu));
    }
    for (k = 0; k < S->nfargs; k++) S->fp[FRB(S)][(4 + k) ^ 1] = b_of(S->fargs[k]);
    S->nfargs = 0;
    S->fpul = rng(S); S->mach = rng(S); S->macl = rng(S); S->t = rng(S) & 1u;
    S->gbr = 0x8c00f000u;
    S->pr = RETURN_MAGIC;
    S->pc = fn;
    memcpy(keep_r, S->r, sizeof keep_r);
    for (k = 0; k < 4; k++) keep_f[k] = S->fp[FRB(S)][12 + k];
    keep_gbr = S->gbr;

    S->cyc = 0; S->slot = 2; S->prev_grp = G_CO; S->fe_free = 0; S->div_free = 0; S->resume = 0; S->need = 0; S->npd = 0;
    S->fe_hold = 0; S->div_hold = 0; S->t_by_mt = 0;
    memset(S->rt_r, 0, sizeof S->rt_r); memset(S->rt_f, 0, sizeof S->rt_f);
    S->rt_t = 0; S->rt_fpul = 0; S->rt_fpul_sts = 0; S->rt_mac = 0; S->rt_pr = 0; S->rt_fpscr = 0;

    while (!S->halt && S->pc != RETURN_MAGIC) step(S);
    S->st.cycles = (uint64_t)(S->resume > S->cyc ? S->resume : S->cyc + 1);
    if (S->halt) return -1;

    for (k = 8; k < 15; k++)
        if (S->r[k] != keep_r[k]) { snprintf(S->err, sizeof S->err, "r%d not restored by the call", k); return -1; }
    if (S->r[15] != sp) { snprintf(S->err, sizeof S->err, "the stack pointer came back as %08x, not %08x", (unsigned)S->r[15], (unsigned)sp); return -1; }
    if ((S->fpscr & FPSCR_MODE) != mode) {
        snprintf(S->err, sizeof S->err, "FPSCR mode bits came back as %06x, not %06x (an frchg or fschg without its pair)", (unsigned)(S->fpscr & FPSCR_MODE), (unsigned)mode);
        return -1;
    }
    for (k = 0; k < 4; k++)
        if (S->fp[FRB(S)][12 + k] != keep_f[k]) { snprintf(S->err, sizeof S->err, "fr%d not restored by the call", 12 + k); return -1; }
    if (S->gbr != keep_gbr) { snprintf(S->err, sizeof S->err, "GBR changed by the call"); return -1; }
    return 0;
}

const char *sh4_error(const sh4_t *S) { return S->err; }
uint32_t sh4_r0(const sh4_t *S) { return S->r[0]; }
float sh4_fr0(const sh4_t *S) { return f_of(S->fp[FRB(S)][0]); }
const sh4_stats_t *sh4_stats(const sh4_t *S) { return &S->st; }
void sh4_seed(sh4_t *S, uint32_t seed) { S->seed = seed ? seed : 1u; }
void sh4_trace(sh4_t *S, int on) { S->trace = on; }
void sh4_movca_poison(sh4_t *S, int on) { S->poison = on; }

void sh4_xmtrx_get(const sh4_t *S, float out[16]) {
    int k;
    for (k = 0; k < 16; k++) out[k] = f_of(S->fp[FRB(S) ^ 1u][k]);
}

void sh4_xmtrx_set(sh4_t *S, const float in[16]) {
    int k;
    for (k = 0; k < 16; k++) S->fp[FRB(S) ^ 1u][k] = b_of(in[k]);
}

void sh4_each_opcode(sh4_t *S, void (*fn)(uint32_t addr, uint16_t opcode, void *user), void *user) {
    int k;
    for (k = 0; k < S->nxsec; k++) {
        uint32_t a;
        for (a = S->xsec[k].addr; a + 2u <= S->xsec[k].addr + S->xsec[k].size; a += 2u) {
            uint16_t op;
            memcpy(&op, S->ram + (a - SH4_RAM_BASE), 2);
            fn(a, op, user);
        }
    }
}
