# SH4ZAM-DCE: how a routine is written, tested and costed here

Read this whole file before you write a line. The worked example of everything below is the `planes` topic: `include/sh4zam_dce/shz_dce_planes.h`, `tests/host/planes_test.c`, `tests/sh4/planes_kernels.c`, `tests/sh4/planes_check.c`, `tests/sh4/planes_jobs.h`, `tests/sh4/planes_bench.c`. Read those six files next and copy their shape.

## 1. What this library is

SH4ZAM-DCE is a sister include of SH4ZAM (Falco Girgis's Dreamcast math library, used as its own library: `third_party/sh4zam` in the repository this folder sits in). Header only, C, `shz_dce_` prefix, one topic a header: `include/sh4zam_dce/shz_dce_<topic>.h`. It is written as a library for anyone: no names of a game, of its files or functions, no local paths, no user names. Plain English in comments (the style of the planes header).

Two kinds of routine:
- New batch routines SH4ZAM does not have.
- Reworked versions of existing SH4ZAM routines, SAME SIGNATURE under a `shz_dce_` name so they drop in, kept only when there is a real gain (cycles, accuracy, fewer stalls). SH4ZAM itself is never edited and its code is never copied: call its public API, or write your own code. (Its headers say: "Do not copy this file or its routines into other projects.")

Two back ends, picked by SH4ZAM's own switch (`SHZ_DCE_SH4` in `shz_dce_cdefs.h`, 1 when `SHZ_BACKEND == SHZ_SH4`):
- SH-4: FTRV, FIPR, FSRRA, FSCA, hand-placed registers, inline asm where the compiler's code is poor.
- Software (the host, any other target): plain C, same results within the bound the comment states.

## 2. The one command

```
python tests/run_checks.py [--repo <repository root>] [--build <build folder>] [--only <topic>] [host] [compile] [sh4] [cycles]
```

Run it with the repository's Python (`tools/bin/cc-venv/Scripts/python`). `--repo` defaults to the repository this folder sits in, `--build` to `<repo>/build/sh4zam-dce`. Give single steps while you iterate. Logs: `<build>/logs/<name>.log`. The SH-4 assembly of a bench file: `<build>/<topic>_bench_O3.s`. The cycle table: `<build>/cycles_<topic>.txt`. Every compiler and test runs hidden, below normal priority, one at a time.

A topic passes when all four steps pass for it:
- `host`: `tests/host/<topic>_test.c` on SH4ZAM's software back end, built twice (strict IEEE flags; `-ffast-math -ffp-contract=fast`), must print `<topic>_test OK`.
- `compile`: `sh-elf-gcc -ml -m4-single`, `-O2` and `-O3`, strict and fast-math, `-Wall -Wextra -Werror`: the header alone, `<topic>_kernels.c`, `<topic>_bench.c`.
- `sh4`: `tests/sh4/<topic>_kernels.c` built for the SH-4 in the four flag sets and RUN by `tests/sh4/<topic>_check.c` through the interpreter; must print `<topic>_check OK`.
- `cycles`: `tests/sh4/<topic>_bench.c` (functions named `bench_*`) through `sh4bench`.

To see where the cycles of a loop go, trace it (0x8c800000 and up is free zeroed RAM; the last number is the count):

```
<build>/sh4run_cli --trace <build>/<topic>_bench_O3.elf _bench_<name> 0x8c900000 0x8c800000 0x8c880000 6
```

Each line: the cycle the instruction issued in, its address, opcode, mnemonic. A jump in the cycle column is a stall: find what it waited for.

## 3. The interpreter (tests/sh4run): what a pass means

`tests/sh4run` executes the compiled SH-4 code on the host, instruction by instruction. It is strict where an emulator is lenient:
- a 16 / 32 / 64-bit access on an address that is not a multiple of 2 / 4 / 8 stops the run (the console's address error: on real hardware a silent hard lock). A pass therefore IS "alignment: checked" for every instruction executed. So give test buffers the WEAKEST alignment the routine's comment allows (`ram(bytes, 8, 4)`: on a multiple of 4, not of 8).
- `movca.l` (a cache line allocated without a read) poisons the other 28 bytes of its 32-byte line: a kernel that allocates a line it does not fill completely, or that a neighbour lives in, fails its compare.
- r8-r14, r15, fr12-fr15, GBR and the FPSCR mode bits (FR, SZ, PR) must come back as they were; every scratch register starts as garbage, NaNs on every other call: run each job twice and compare (a result that changes read a register it never wrote).
- FIPR / FTRV / FSRRA / FSCA give the console's own bits when the third party's bit-exact models are present (`tests/third_party/sh4_fpu_approx`), plain sums and 1 / sqrt otherwise. Either way a test of an approximate routine compares against the exact value within the stated bound, never bit for bit against the host's software back end.
- an instruction it does not model stops the run and names it. Do not use `mac.l`, `tas.b`, GBR-relative byte logic, store queues (0xe0000000) or double precision in kernels.

The cycle count is an ESTIMATE from a model of the SH7750 pipeline (section 4), every memory access a cache hit. Every figure in a comment or a report is "E" (estimate), never "measured".

## 4. The SH-4 as a kernel writer must see it

Pipeline: two instructions issue per cycle, IN ORDER, when their groups allow it:
- MT (mov reg / imm, cmp/xx, tst, exts / extu, nop): pairs with anything, even another MT. mov is latency 0; the others 1. A compare followed by its conditional branch share a cycle.
- EX (add, sub, and, or, xor, shifts, rotcl / rotcr, movt, dt, neg): ONE per cycle. Latency 1.
- LS (every load and store, fmov reg-reg, fldi0 / fldi1, fneg, fabs, flds, fsts, lds / sts fpul, pref, movca): ONE per cycle. A load's value is ready 2 cycles later (a post-increment pointer after 1). fmov reg-reg, fldi, fneg, fabs: latency 0, but they still take the LS slot.
- FE (fadd, fsub, fmul, fmac, fcmp, float, ftrc, fipr, ftrv, fsrra, fsca, frchg, fschg): ONE per cycle.
- BR (bt, bf, bra and the /s forms): one per cycle; a taken branch costs 2 cycles (the target issues 2 cycles after the branch; jmp / rts 3). A delay slot instruction is free work.
- CO (mul.l, lds / sts mach / macl / pr, jsr, rts ...): alone, 2 or more cycles.

Latencies (cycles from issue until the result can be used): fadd / fsub / fmul / fmac / float / ftrc 3; fcmp sets T after 2; FIPR 4; FTRV 5, 5, 6, 7 for its four results and it keeps the FPU busy 4 cycles (no other FE instruction can issue); FSRRA 5, busy 3 (the busy time is assumed); FSCA 6 / 7, busy 4 (assumed); fdiv 12; fsqrt 11; sts fpul 3; lds fpul 1.

Consequences that decide every design here:
1. The machine is in order. An instruction whose operand is not ready stalls itself AND everything behind it. A loop "load, ftrv, store" costs the whole latency chain per element. Hide latency by interleaving independent work between a producer and its consumer: the next element's loads, the previous element's stores (software pipelining), or two elements per iteration.
2. Count the scarce slots per element before you write: LS (loads + stores + register moves + constants), FE, EX are each one per cycle, and two instructions issue per cycle at most. The floor of the loop is the larger of the largest of the three counts and half the instruction count, whatever you do.
3. There is ONE T bit and fcmp sets it 2 cycles later: n float compares whose results are collected (`movt` / `rotcl` / `rotcr`) cost 2n cycles in a chain. Put other work in the gaps.
4. FIPR and FTRV want their operands in fv0 / fv4 / fv8 / fv12 (fr0-3, fr4-7, fr8-11, fr12-15). FIPR writes its result over the LAST element of its second operand. Load straight from memory into the right registers; every fmov between registers is an LS slot.
5. With FPSCR.SZ = 1 (`fschg`), every fmov is a PAIR of floats: 8 bytes per load / store, one LS slot for two values, and it can load straight into the second bank (`xd0`..). The address must be a multiple of 8. Toggle SZ once around a loop, not per element, and put it back.
6. The compiler copies SH4ZAM's by-value vector / quaternion structs (unions inside) through the stack by integer moves: a loop `out[i] = f(a[i], b[i])` over `shz_vec3_t` / `shz_quat_t` pays 2 to 5 extra loads and stores per struct. Batch routines take pointers and read the floats directly.
7. Two SH4ZAM primitives that both bind hard registers (`shz_xmtrx_transform_vec4` then `shz_vec4_dot2`) make the compiler spill to the stack. One planned asm block does not.
8. A test in front of a long operation holds it back: `x == 0 ? 0 : f(FSRRA(x))` compiles to compare, branch, FSRRA, and the branch waits 2 cycles for T with FSRRA queued behind it (3 to 4 cycles a call). Compute first and test in the operation's shadow: `r = f(FSRRA(x)); if (x == 0) r = 0;` (`shz_dce_sqrt.h`). The FPU's exception enables must be off for the discarded result (KallistiOS's default). THE MODEL ONLY: on a console, single calls of these reworks were a cycle or two slower, not faster (`doc/THESIS.md`, section 7). A gain the model shows for a single-call rework is a hypothesis until a console has measured it.
9. The order of instructions is the compiler's, not the source's. It emits a dependent multiply right behind FSRRA and queues independent work after it; with `-ffast-math` it also regroups products and sums by its own rules (it turns `inv * (a * p)` back into `(inv * a) * p`) and may hand back an operand that compared equal to a constant instead of the constant. `shz_dce_opaquef()` (an empty asm statement, no instruction) stops the regrouping and the substitution at one value, and it also keeps whatever feeds it in front of a following branch (the compiler does not sink an asm statement), which is a cost on the side of the branch that did not need it; only an asm block fixes an order.
10. A cycle whose two slots go to an EX and an FE instruction (a T reader beside the next compare: `movt` / `rotcl` with `fcmp`) has no room for a load or store: a chain of n collected compares takes about n LS slots out of the loop, on top of rule 2's count. When LS is the scarce slot that is the floor: `shz_dce_project.h` is 27 LS + 5 such cycles + the loop branch = 33, one vertex at a time, and a second vertex in flight would not lower it. Loads may run ahead of the test that decides whether their element is wanted (that loop loads the position of a vertex it then skips): its transform issues 4 cycles sooner.

Arithmetic facts:
- FTRV and FIPR are APPROXIMATE on the SH-4 (manual: error <= largest product x 2^-23 + |result| x 2^-23) and round once: never bit-identical to a scalar sum. FIPR(a, b) and FIPR(b, a) may differ in the last bits (FVm is the multiplicand, FVn / the XMTRX element goes through a truncated multiplier); an FTRV row equals `fipr vector, row`.
- FSRRA is approximate: measure its error, never assume it exact.
- `shz_vec3_dot`, `shz_vec4_dot`, `shz_dot6f / 8f`, `shz_mag_sqr3f / 4f` are plain fmul + fmac on the SH-4 (SH4ZAM's FIPR toggle `SHZ_SH4_DOT_FIPR` is off by default): they are NOT FIPR.
- With `-ffast-math -ffp-contract=fast` the compiler fuses a * b + c into fmac and may reorder sums. "Bit-identical to the scalar form" can only be promised when every product is exact or there is a single rounding (integer work, table lookups, one multiply, a power-of-two scale). Otherwise the answer is NO, and say why. The one way to a YES across a fused sum: write the fused step out (`shz_dce_vertex_fmaf`), in the form the compiler makes of the plain expression, and let the SH-4 check compare the bytes with the plain expression compiled in the builds that contract (`shz_dce_project.h`).
- KallistiOS runs with FPSCR.DN = 1 (denormals are zero) and round to nearest. `isnan` / `isfinite` / `x != x` fold away under `-ffast-math`: test bits instead.

Inline asm (GCC, SH):
- One `asm volatile(R"( ... )" : outputs : inputs : clobbers);` block for a whole loop. Operands `[name] "+r"` (in-out general register), `"r"`, `"=&r"` (scratch). List every register you name yourself in the clobbers: `"r0"`, `"fr0"`.. `"fr15"`, `"t"`, `"fpul"`, `"macl"`, and `"memory"` when you load or store through pointers. A clobbered fr12-fr15 is saved by the compiler (fine for a batch routine).
- `mov.b / mov.w` with a displacement only work through r0 (`mov.b r0, @(1, rN)`); float loads have no displacement form: `@rN`, `@rN+`, `@(r0, rN)`; float stores `@rN`, `@-rN`, `@(r0, rN)`.
- `fcmp/gt frM, frN` sets T = (frN > frM). `fsub frM, frN`: frN -= frM. `fmac fr0, frM, frN`: frN += fr0 * frM.
- Never `.macro` inside a header (it is assembled once per instantiation): build repeated sequences with C preprocessor string macros, and `#undef` them.
- Leave FPSCR as you found it (every fschg / frchg paired on every path), leave XMTRX alone unless the routine's comment says it overwrites it.

## 5. Shape of a header

Follow `shz_dce_planes.h`:
- banner, exactly the planes header's: `SH4ZAM-DCE: <topic>`, the file name, `Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).`, `Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.`, `MIT License: see ../../LICENSE.` No other copyright line and no author line anywhere. Then a `\file` comment in plain words: what the routines do and the idea behind them, call order, alignment, error bound, bit-identical yes / no and why, where it pays and where it does not (with cycles E), `Version: v0.10`, "The effect / Use it for".
- `#include "shz_dce_cdefs.h"`, `extern "C"` guards, types, then per routine a `\brief` comment that states: inputs, what it overwrites, alignment of every pointer, error bound, bit-identical yes / no, cycles E.
- Per batch routine three functions: `shz_dce_<name>_c` (plain C on SH4ZAM's public API: the software back end and the reference), `shz_dce_<name>_sh4` (inside `#if SHZ_DCE_SH4`, asm), and `shz_dce_<name>` (asserts the alignment with `SHZ_DCE_ASSERT_ALIGNED`, dispatches). A routine whose C form already compiles to good SH-4 code needs no asm: say so with the numbers.
- `SHZ_INLINE` (static inline) for batch routines, `SHZ_FORCE_INLINE` for tiny ones, `SHZ_NOEXCEPT`, `SHZ_RESTRICT` on batch pointers.
- A drop-in rework of an SH4ZAM routine keeps its exact signature and semantics (same special cases) and says in its comment: that it is a rework of SH4ZAM's `shz_<name>` (Falco Girgis), what the original does, what this changes, cycles E before / after, error bound against the original, bit-identical to the original yes / no. It gets a line in `shz_dce_override.h` (the macro that puts SH4ZAM's name on it): in the first group when it returns SH4ZAM's bits, in the `SHZ_DCE_OVERRIDE_APPROX` group otherwise, and a line in `tests/host/override_test.c`.

## 6. Shape of the tests

- `tests/host/<topic>_test.c` (include `shz_dce_test.h`): against an EXACT reference computed in double (or integers) by the test itself; `T_CHECK` every claim of the header's comment: the bound, the special cases, the batch against the single form for every count from 0 up (odd counts, count 1) and every stride / alignment the comment allows, no write past the end (guard bytes). For "not bit-identical" routines print how often the result differs from the scalar form and prove every difference is inside the bound. For bit-identical routines `memcmp` against the scalar form over a large sweep. Must pass with both flag sets. Ends with `return t_done("<topic>_test");`. A fused multiply-add in a reference is `shz_dce_vertex_fmaf` (checked against integers in `vertex_test.c`), never the C library's `fmaf`: the one of the host compiler used here returns a neighbour of the right float in a quarter of all cases.
- `tests/sh4/<topic>_jobs.h`: one struct of `uint32_t` fields per job (SH-4 addresses and numbers).
- `tests/sh4/<topic>_kernels.c` (SH-4 side): includes ONLY the topic's header and its jobs header; one `void t_<name>(job_<name>_t *j)` per job that runs the routine, its `_c` form and (for reworks) the original SH4ZAM routine on the same input into separate outputs. It must call every public routine of the header (it is the compile check).
- `tests/sh4/<topic>_check.c` (host side, include `sh4_check.h`): fills jobs with `ram()` / `host()`, `call("_t_<name>", job)`, then checks: asm form == C form bit for bit when they execute the same FTRV / FIPR / exact arithmetic; results against the exact value within the bound; a second `call` gives the same bytes; guard bytes intact; XMTRX unchanged (`sh4_xmtrx_get`) unless documented. Ends with `return t_done("<topic>_check");`.
- `tests/sh4/<topic>_bench.c`: `void bench_<name>(out, in0, in1, size_t n [, float t])` loops: the routine, its `_c` form, and the code it replaces (the SH4ZAM original or the plain scalar loop), so the table reads before / after. Inputs are random floats in [-1, 1); `out`, `in0`, `in1` are on multiples of 32, 64 bytes an element at most.

## 7. What a finished routine reports

Per routine: one line of what it does; alignment; error bound; bit-identical yes / no; cycles E before -> after (name the baseline row of `cycles_<topic>.txt`); what the gain comes from. Also what was tried and did NOT pay, with its numbers: that is a result too. And every claim that could not be verified here (a console's timing above all).
