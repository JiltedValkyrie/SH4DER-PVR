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

## 8. Routines written under this guide: the shadow compare (`shz_dce_shadow.h`)

`shz_dce_shadow_compare_batch` is a stop-gap: when SH4ZAM has its own version, callers move to it and this header is removed.

The first routine written to this guide after section 7: `shz_dce_shadow_compare_batch`. Its planar compare of one point is the one a shadow map needs; the library's per-point loop is what it replaces. The header's own comment has the algorithm; this section has the record.

**What it does.** N points in, N lit bytes out (255 lit, 0 shadowed). Per point: one FTRV of (x, y, z, 1) through the map's matrix (in XMTRX, loaded by the caller); the sign-safe reciprocal of W; the texel size for the bias when `bias_b != 0`; the 2 x 2 solve for the depth per texel; one texel compare, or four blended (pcf). The two surface axes come in already transformed (FTRV of (t, 0), once per batch), so the per-point code makes one transform, not three.

**Alignment.** Points: floats on a multiple of 4, stride a multiple of 4 and at least 12. Depth: uint16_t on a multiple of 2. g1, g2: float[4] on a multiple of 4. Output: any address, one byte a point at any stride of at least 1. XMTRX is READ, not changed: the caller loads the map's matrix; on return XMTRX still holds it (checked on the host and on the SH-4 interpreter after every batch).

**Error bound.** Software back end: plain float arithmetic through SH4ZAM's public API. SH-4: the point's transform is FTRV (bound (P + |R|) x 2^-21). The divides are SH4ZAM's `shz_divf`, which is `num x shz_invf(denom)`: on the SH-4 an FSRRA reciprocal, on the software back end `1 / sqrt(x x)`. Neither is an exact division. A point within these bounds of a texel edge, a depth level or the singularity can fall the other way; the test measures that as a margin, it is not an error bound against the exact value.

**Bit-identical.** `_c` against the planar loop it replaces, point for point, on the software back end. Measured:
- strict IEEE (`-fno-fast-math -ffp-contract=off`): YES. The host test compares the two on 96,000 random points (0 differ, `shadow_test`). The scene harness (`psm_shadow_identity_test`, scratch) compares 72 configurations of the demo scene (three map sizes, warp on and off, pcf on and off, bias 0 and 1.5, three suns) plus random points (NaN, infinite, huge, both signs of W, identical and zero-length axes, the non-planar call): 18,101,592 bytes identical.
- `-ffast-math -fno-unsafe-math-optimizations`: YES in both (0 differ in the host test; the scene harness identical).
- plain `-ffast-math`: NOT claimed. The host test: 30 of 96,000 points differ. The scene harness: 16 of 18,101,592 bytes differ (the first at config 12, ground vertex 892). Mechanism, as far as it was followed: `-funsafe-math-optimizations` folds SH4ZAM's software reciprocal (`shz_divf` is `num * shz_invf(denom)`, and on the software back end `1 / sqrt(x x)`) differently in the two inline contexts. Disabling `-freciprocal-math` alone did not restore identity in the host test (22 differ); disabling `-funsafe-math-optimizations` did. Putting an `shz_dce_opaquef` barrier on the kernel's reciprocal moved the difference to another byte rather than removing it, so it was reverted. Not fixed. The DC build uses `-ffast-math`, but there the software back end is not used: the SH-4 is. On the SH-4: the `_sh4` form is the same C (below), so `_sh4`, `_c` and the dispatch agree bit for bit; against the planar loop run on the SH-4 interpreter they agree except (a) identical axes, where the planar loop's determinant is a rounding residual of the contracted products and not zero, and the routine takes the fallback (a test of the axes, now in the code); and (b) bytes inside a margin.

**The `_sh4` form.** No hand asm. `_sh4` is the same C, and SH4ZAM's `shz_xmtrx_transform_vec4` is the FTRV instruction in the SH-4 build, so the body needs no asm of its own. Asm was not justified without console data. The divides stay sign-safe (`shz_divf`: W, the texel measure and the determinant can be negative); no fast reciprocal is used, because no denominator's sign is proven positive except the texel area, and that one is still divided.

**Cycles E** (the interpreter's model, cache hits assumed; from `build/sh4zam-dce/cycles_shadow.txt`; per point). Dynamic instructions per point in brackets.

| Baseline row (planar loop it replaces) | E a point |
|---|---|
| `-O3 -ffast-math`, pcf 1 and texel bias 1.5: `_bench_shadow_planar_before` | 634 (636) |
| the routine, `_bench_shadow_compare` | 489 (464) |
| `-O2 -ffast-math`, same case: planar | 677 (690) |
| the routine | 647 (651) |
| `-O3 -ffast-math`, pcf 0 and bias 0: planar | 270 (298) |
| the routine | 184 (176) |
| `-O2 -ffast-math`, same case: planar | 281 (311) |
| the routine | 282 (320) |

Loads and stores a point, `-O3` pcf 1 bias 1.5: routine 64.5 and 10.9, planar 104.3 and 40.5. The `_c` form measures 492 and 646 (`-O3` and `-O2`), within the variation of code layout of the dispatch.

**Static instruction lines** (the `-S` output of the same bench file, `build/sh4zam-dce/shadow_bench_O{2,3}.s`; a routine's lines plus the out-of-line helpers it calls; setup included, so NOT per point): `-O2`: routine 189 + `plane_` 330 + `lit_` 298 + `texel_` 159 + dispatch 10 = 986; planar 43 + `before_batch` 879 = 922. `-O3`: routine 1,162 (the helpers are inlined); planar 1,202.

**Where the gain comes from.** Fewer transforms and fewer copies. The planar loop does three FTRVs a point and copies each result through memory (40.5 stores a point at `-O3`, against 10.9 for the routine). The routine does one FTRV a point and reads the two axes from a block filled once a batch. The FSRRA count is the same on both sides (6 a point at `-O3`); the rest of the instruction mix differs by the transforms' products and copies. The gain is 23% at `-O3` and 4% at `-O2`; why `-O2` gains less was not examined.

**What did not pay, with numbers.** The plain case (pcf 0, bias 0) at `-O2`: 282 against 281, no gain. The separate `_sh4` asm: not written, so there is no measurement of one; the `_c` and dispatch forms differ by 1 to 3 E in the table, which is the layout's noise. The identical-axes test is a per-point compare of four floats; it is not separately measured, and hoisting it per batch (the axes are per batch) was not done. A fast FSRRA reciprocal for the positive texel area was not tried (it would change bits; its gain is unmeasured). `shz_dce_gather.h` was evaluated and does not fit: its routines unpack bit-packed 3-field vertices and copy 12-byte records; the shadow's taps are 2 x 2 reads of a uint16 map with an out-of-map rule, and no interface of it covers that.

**Host proxy (x86, not the SH-4, not a console).** Software back end, ns a point, best of three, `-O3`: routine 452 ns, planar 1,306 ns (pcf 1, bias 1.5); plain 446 against 1,257. The two routines' bytes are identical on the host (checked). The gap is much larger than the SH-4 model's. A likely cause is the software back end's XMTRX, which lives in thread-local state that each transform reads (the planar loop makes three transforms a point); this was not checked. Proxy only.

**Not measured, and not claimed.** Any Dreamcast or emulator time (none run). Cache misses on the SH-4: the map (32 KB at 128 x 128) and the depth reads are random access; the model assumes every access hits. The SH-4 FTRV and FSRRA bits: the interpreter used the third party's bit-exact unit models (`tests/third_party/sh4_fpu_approx`, present here), which are a model of the console, not the console. The planar loop's identity on the SH-4 beyond the 1,760 SH-4 points and 150 batches of the check. The byte-identity claim for `-ffast-math` on other compilers. The cost of the scene in the demo's frame budget.

**Credit.** The steps are those of the planar shadow-map compare in SH4DER-PVR (Copyright (c) 2026 Jilted Valkyrie); see README, Acknowledgments.

## Entry: tri_row (shz_dce_tri_row.h, one triangle row into a 16-bit depth row)

`shz_dce_tri_row_step` is a stop-gap: when SH4ZAM has its own version, callers move to it and this header is removed.

The routines: `shz_dce_tri_row_step_c` (the running-add row, unchanged, the reference), `shz_dce_tri_row_step_sh4` (the same edges by running adds; a covered texel's depth by one FIPR each, barycentric) and the dispatch `shz_dce_tri_row_step`. The BEFORE is the running-add row of the shadow raster it came from (kept as a copy in the bench, as the guide asks).

Instruction counts per texel, one path at a time. Two sets, labelled differently:
- static, for the hand-written SH-4 loop: the instructions on the path, counted from its text (the perspective covered path runs the guard, the reciprocal and the store; the rejected path is the first edge test and the advance).
- executed, for the compiled C forms and the BEFORE: the instructions the interpreter ran per texel (sh4bench's `instrs` column, the same compiled code the cycle figures come from, built with -ffast-math -mfsrra -mfsca at -O2 and -O3).

| path | BEFORE (running-add row), executed, -O3 / -O2 | C form, executed | SH-4 loop, static | SH-4 loop, executed |
|---|---|---|---|---|
| perspective, covered | 34.5 / 42.5 | 34 | 35 | 34 |
| perspective, rejected | 15 / 15 | 15 | 9 | 9 |
| flat, covered | 27 / 31 | 27 | 27 | 26 |
| flat, rejected | 14 / 15 | 14 | 9 | 9 |

Cycles, E (the interpreter's pipeline model, a texel; -O3 / -O2):

| path | BEFORE | C form | SH-4 loop |
|---|---|---|---|
| perspective, covered | 43 / 50 | 43 / 43 | 44 / 44 |
| perspective, rejected | 12 / 12 | 12 / 12 | 10 / 10 |
| flat, covered | 27 / 32 | 27 / 27 | 30 / 30 |
| flat, rejected | 11 / 12 | 11 / 11 | 10 / 10 |

What the counts show, plainly: a win on the rejected texels only (12 -> 10 cycles, about 17% fewer, at both levels). On covered texels there is no win. Perspective covered is 12% fewer at -O2 but 2% more at -O3 against the BEFORE; flat covered is 6% fewer at -O2 and 11% more at -O3. The SH-4 loop has fewer instructions on the rejected path (9 against 14 to 15), and that is what the cycles show; the covered path has the same count of adds and more FIPR-and-guard work, so it does not come out ahead. A mix of 80% rejected and 20% covered, perspective, gives about 16.8 cycles a texel for the SH-4 loop against 18.2 for the C form (about 8% fewer). That mix is an assumption, not a measured share. The verdict: a modest model-level gain, adopted only behind a switch, default off, until a console measures it.

Not measured, and said so: no Dreamcast and no emulator were used. Every cycle figure is the interpreter's pipeline model (E), not a console. Not measured: the real FIPR and FSRRA latencies and their pipelining, the load and store pairing with the FP unit, the cache, the branch costs, and the accuracy of FIPR and FSRRA on hardware. The checks ran the loops on the vendored bit-exact unit models in tests/third_party/sh4_fpu_approx (the runner's own models, not a console's), so the FIPR and FSRRA results are checked against that model, not the silicon.

Bit identity, stated where it holds: the C form is bit-identical to the running-add row under strict IEEE flags (the host test; every texel of 20000 random triangles and the reference row for every count from 0 to 256). Under -ffast-math the contraction of the same expressions can differ (measured: 50128 texels of the same triangles, none in coverage, largest 748 units). The SH-4 loop is not bit-identical to either, by design (a dot product against running sums); its coverage is the running sums' own.

## Entry: pvr (shz_dce_pvr.h, PVR vertices through XMTRX with the perspective divide)

The routines: `shz_dce_pvr_project_c` (plain C on SH4ZAM's `shz_xmtrx_transform_vec4` and `shz_invf_fsrra`: the reference and the software back end), `shz_dce_pvr_project_sh4` (one asm block, two vertices in flight) and the dispatch `shz_dce_pvr_project`. The BEFORE is the plain loop: the C form, and the same loop with a divide for 1 / W.

**What it does.** N 32-byte PVR vertices in (command word, x y z in model or world space, four words carried), N out: the same records with (X / W, Y / W, 1 / W) where the position was, (X, Y, Z, W) = XMTRX (x, y, z, 1), 1 / W = FSRRA(W W). The output is a block the tile accelerator takes as it is.

**Alignment.** in: a multiple of 4. out: a multiple of 32 for the SH-4 loop (it allocates each record's cache line and writes all 32 bytes; off a multiple of 32 the C form runs). No overlap: the line is allocated before the source's last four words are read. XMTRX is read, not changed. Every W must be over 0.

**Error bound.** FTRV's and FSRRA's own; none added. The checks hold the screen point to 2 x ((P + |X|) / W x 2^-21 + |X / W| x (W's own FTRV bound + 4 x 2^-21)) of the exact quotient.

**Bit-identical.** YES against the C form on the SH-4, all 32 bytes of every record, counts 0 to 41 and 257, in the four flag sets (`pvr_check`). The same FTRV, the same FSRRA of the same product, one multiply each.

**Cycles E** (`build/sh4zam-dce/cycles_pvr.txt`, a vertex, the same at -O2 and -O3):

| Row | E a vertex | instructions |
|---|---|---|
| `_bench_pvr_project_divide` (plain loop, FDIV) | 39.0 | 38 |
| `_bench_pvr_project_c` (plain loop, FSRRA) | 39.0 | 39 to 41 |
| `_bench_pvr_project` | 19.5 | 30 |

**Where the gain comes from.** The plain loop is the latency chain end to end: transform 7, square 3, reciprocal root 5, product 3, with the loads before and the stores after. The loop here begins the next vertex (its eight loads, its transform, its line and five stores) inside that wait, with the two vertices in fv4 and fv8 in turn so neither waits for a register. A step is 18 loads, stores and moves, one a cycle, and nothing else sets the pace; the loop branch adds 1.5.

**What was tried and did not pay, with numbers.** The first form copied W to a scratch register and walked a second pointer for the screen point's stores: 22.0. Squaring W in place and storing through `@(r0, q)` with constant offsets took the copy and two trailing adds out: 19.5. Pair moves (FPSCR.SZ = 1) for the carried words would take four more loads and stores out; not tried: the position sits across a pair boundary (command word and x share a pair), so it needs a permuted XMTRX or two FSCHG a vertex.

**Not measured, and not claimed.** Any console time: the spike `demo/psm/compare/c4_spike.c` (page 5) times this loop against its C form on a console and compares the bytes; until its pages exist the 19.5 is the model's. Cache misses (the model assumes hits; the loop prefetches the input 96 bytes ahead and allocates the output lines).
