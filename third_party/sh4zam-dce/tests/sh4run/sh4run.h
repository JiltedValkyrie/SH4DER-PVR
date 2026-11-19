/* SH4ZAM-DCE test tool: sh4run

   sh4run.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    sh4run.h
    \brief   A small SH-4 user-mode interpreter: runs the code sh-elf-gcc made for the Dreamcast
             on a host, one instruction at a time, so a math kernel can be checked without a console.

    What it is for. A header of Dreamcast math is compiled twice: for the host (the software back
    end) and for the SH-4 (FTRV, FIPR, FSRRA, hand-placed registers). Only the first can run in a
    host test, and an emulator forgives what a console does not. This runs the second:

    - Every load and store is checked the way the console checks it: a 16-bit access on an odd
      address, a 32-bit access off a multiple of 4, a 64-bit (FPSCR.SZ = 1) access off a multiple
      of 8 is an address error and stops the run with the instruction's address.
    - MOVCA.L poisons the other 28 bytes of its 32-byte line, as a cache line allocated without a
      read holds no defined data: a kernel that allocates a line it does not fill, or one a
      neighbour lives in, fails its compare.
    - A call must leave r8-r14, r15, fr12-fr15, GBR and the FPSCR mode bits (FR, SZ, PR, DN, RM) as
      it found them, and every scratch register starts as garbage from a seed (run twice with two
      seeds: a result that changes read a register it never wrote).
    - FIPR, FTRV, FSRRA and FSCA come from bit-exact models of the SH-4's vector unit when the
      build has them (SH4RUN_HW_MODEL, see sh4run.c), else from plain float sums.
    - A cycle ESTIMATE from a simplified model of the SH7750 pipeline (two instructions a cycle by
      group, latencies, the FPU's busy times, taken branches; every memory access a cache hit).
      It is an estimate for comparing two versions of a routine, not a measurement.

    Not an emulator: no interrupts, no MMU, no hardware registers, no store queues, user-mode
    instructions only. An instruction it does not know stops the run and says so.

    Version: v0.10
*/

#ifndef SH4RUN_H
#define SH4RUN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SH4_RAM_BASE 0x8c000000u   /**< \brief Start of the simulated RAM (the console's, cached area P1). */
#define SH4_RAM_SIZE 0x01000000u   /**< \brief 16 MB. */

/** \brief Counters of the last sh4_call(). */
typedef struct sh4_stats {
    uint64_t instrs;         /**< \brief Instructions executed. */
    uint64_t cycles;         /**< \brief Pipeline model ESTIMATE, call entry to the return's target. */
    uint32_t fipr, ftrv, fsrra, fsca, fmac, fdiv, fsqrt, fmul, fadd, fsub, fcmp, cvt;
    uint32_t loads, stores, taken, movca, pref, mode;   /**< \brief mode: FRCHG + FSCHG. */
    uint32_t denorm;         /**< \brief Float operands or results flushed to zero (FPSCR.DN = 1). */
} sh4_stats_t;

typedef struct sh4 sh4_t;

/** \brief A machine with zeroed RAM, or NULL. */
sh4_t *sh4_new(void);
void sh4_free(sh4_t *s);

/** \brief Load a linked ELF (elf32-shl): its allocated sections into RAM, its symbols into a table. 0: loaded. */
int sh4_load_elf(sh4_t *s, const char *path);

/** \brief Address of a symbol (C name with the leading underscore, as the object file spells it), 0: none. */
uint32_t sh4_sym(const sh4_t *s, const char *name);

/** \brief Bytes of simulated RAM for test data, aligned; 0: out of room. sh4_alloc_reset frees them all. */
uint32_t sh4_alloc(sh4_t *s, uint32_t bytes, uint32_t align);
void sh4_alloc_reset(sh4_t *s);

/** \brief Host pointer to bytes of simulated RAM (the host is little-endian, as the SH-4 here), NULL: out of range. */
void *sh4_ptr(sh4_t *s, uint32_t addr, uint32_t bytes);

/** \brief Call the function at fn with up to four integer or pointer arguments (r4-r7) and the float
    arguments set by sh4_float_args() (cleared by the call). 0: it returned and kept the calling
    convention; else sh4_error() says what went wrong. */
int sh4_call(sh4_t *s, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/** \brief Float arguments of the next call, in order (the little-endian SH-4 ABI puts them in fr5, fr4, fr7, fr6, ...; at most 8). */
void sh4_float_args(sh4_t *s, const float *args, int count);

const char *sh4_error(const sh4_t *s);
uint32_t sh4_r0(const sh4_t *s);              /**< \brief Integer result of the last call. */
float sh4_fr0(const sh4_t *s);                /**< \brief Float result of the last call. */
const sh4_stats_t *sh4_stats(const sh4_t *s);

void sh4_seed(sh4_t *s, uint32_t seed);       /**< \brief Garbage for the scratch registers at the next calls' entry. */
void sh4_xmtrx_get(const sh4_t *s, float out[16]);   /**< \brief XF0..XF15 (column major). */
void sh4_xmtrx_set(sh4_t *s, const float in[16]);
void sh4_trace(sh4_t *s, int on);             /**< \brief Print every instruction with its issue cycle (stderr). */
void sh4_movca_poison(sh4_t *s, int on);      /**< \brief On by default. */
int sh4_hw_model(void);                       /**< \brief 1: built with the bit-exact FIPR / FTRV / FSRRA / FSCA models. */

/** \brief Mnemonic of a 16-bit opcode as GNU objdump prints it ("?" for one this tool does not know). */
const char *sh4_mnemonic(uint16_t opcode);

/** \brief Walk the executable sections of the loaded ELF: fn(address, opcode, user) per 16-bit word. */
void sh4_each_opcode(sh4_t *s, void (*fn)(uint32_t addr, uint16_t opcode, void *user), void *user);

#ifdef __cplusplus
}
#endif

#endif /* SH4RUN_H */
