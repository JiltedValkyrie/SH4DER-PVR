# SH4ZAM-DCE: a sister include of SH4ZAM, for review

Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam). MIT, copyright line SH4ZAM's own ("Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors"). SH4ZAM is used as its own library, version 0.10.0, unmodified; none of its code is copied.

Revision 2 (2026-11-18): adds the aliasing note of 1.4 (the output record type is may_alias), finding 4.11 with suggestion 8, and the run-time-width figures of 1.5.
Revision 3 (2026-11-18): adds `shz_dce_project_vertices` (the table's row, bound (B5), section 1.8, its test counts and the note on the host's `fmaf` in section 3).
Revision 4 (2026-11-18): a console's results (section 7, reported by Falco Girgis) and what they change: the reordered roots lose there, so 2.1, 2.4, 4.3, 4.5 and suggestion 1 are rewritten and the override header has a switch a group; the arcsine and arccosine stand, without the value barrier (2.2), and as one file for upstream (`doc/upstream/shz_asin_acos.h`). Appendix B says exactly what the value barrier does. The interpreter now gives the SH-4's one quiet NaN (section 3).
Revision 5 (2026-11-18): `shz_atan2f`, on your question (faster and / or more accurate): where its error comes from, a rework that is closer by a factor of 5000 for 9 cycles more in the model, and why nothing faster came of it in C (the table's row, section 2.5, finding 4.12, suggestion 9, Q7, the test counts).

What this is: a header-only include (`include/sh4zam_dce/shz_dce_*.h`, prefix `shz_dce_`) holding (a) batch routines SH4ZAM does not have and (b) reworks of some SH4ZAM routines under the same signature. It was written for a Dreamcast port of a 3D game and is game-agnostic; the places it serves are a world (BSP) vertex transform, a character mover that reads a compressed collision mesh, a skeleton blend, and, in a second project, the fused transform / clip / project pass of world and model vertices. Everything below was produced without a console: the SH-4 code is compiled by sh-elf-gcc 15.2.0 (`-ml -m4-single`, `-O2` and `-O3`, with and without `-ffast-math -ffp-contract=fast -mfsrra -mfsca`) and executed instruction by instruction in an interpreter written for the purpose (section 3). Every cycle figure is therefore an ESTIMATE, written "E", from a pipeline model, cache hits assumed. Section 6 lists what only you or a console can confirm.

## 0. Notation and the machine model

- fl(x): x rounded to the nearest float; u = 2^-24. ulp(x) = 2^(e-23) for |x| in [2^e, 2^(e+1)).
- For an inner product of four terms, R = sum a_i b_i (exact) and P = max_i |a_i b_i|.
- beta(P, R) = (P + |R|) x 2^-21: the error bound this library states for any four-term inner product, on either back end (derived in 1.1).
- eps_f: relative error of FSRRA. From the bit-exact model (section 3), over all 2^24 (mantissa, exponent parity) cases, round to nearest: max |FSRRA(x) sqrt(x) - 1| = 1.85e-7 = 2^-22.37 (at x = 0x4044039c), worst 1.99 ulp, 33.0% of results are not the nearest float, 16,772 are off by more than one float. FSCA over all 65,536 angles: max |error| = 1.12e-7 = 2^-23.09 in sine and in cosine. FIPR over 4 x 10^6 random cases (including sums that cancel): max error 0.419 (P + |R|) 2^-23, none over the manual's (P + |R|) 2^-23; FIPR(a, b) != FIPR(b, a) in 3.5% of them.
- FPSCR as KallistiOS leaves it: DN = 1 (a denormal operand or result is a zero of its sign), RM = nearest, PR = 0, SZ = 0, FR = 0, all exception enables off.

Pipeline model (SH7750 hardware manual's instruction groups; the starred values are assumptions, see question Q1). Instructions issue in program order, at most two a cycle. Groups: MT (mov, cmp, tst, nop: pair with anything), EX (add, shifts, movt, dt: one a cycle), LS (every load, store, fmov, fldi, fabs, fneg, lds/sts FPUL, pref, movca: one a cycle), FE (fadd fsub fmul fmac fcmp float ftrc fipr ftrv fsrra fsca fschg: one a cycle), BR (one a cycle), CO (alone). Instruction i issues at

    t_i = max( t_(i-1) + 1 - p_i ,  max over sources s of r(s) ,  F if i is FE ,  b )

with p_i = 1 when i can share the cycle of i-1 (second slot, groups compatible), r(s) the cycle a source value is ready, F the cycle the FE stage is free, b the resume cycle after a taken branch (t_branch + 2; the delay slot issues in between). Each destination d gets r(d) = t_i + L, and for an FE instruction F = t_i + h:

    L:  load 2 (post-increment pointer 1) | fmov reg, fldi, fabs, fneg 0 | EX 1 | fadd fsub fmul fmac float ftrc 3 | fcmp -> T 2
        FIPR 4 | FTRV 5, 5, 6, 7 for its four lanes | FSRRA 5* | FSCA 6, 7* | fdiv 12 | fsqrt 11 | sts fpul 3 | lds fpul 1
    h:  1, except FTRV 4, FSRRA 3*, FSCA 4*

The cost of a loop is C = (t(n2) - t(n1)) / (n2 - n1) cycles an element, n1 = 64, n2 = 192. These bounds decide every design here, for a body of N instructions with n_LS, n_FE, n_EX of each scarce group:

    (B1) slots:   C >= max( n_LS , n_EX , n_FE + sum (h - 1) , ceil(N / 2) )
    (B2) serial:  written "load, compute, store" per element, C ~ sum of L along the element's longest path, because the
                  consumer of a late value stalls itself AND everything issued after it (in order)
    (B3) k elements in flight (their paths interleaved in program order): C >= max( (B1) , path / k )
    (B4) one T bit: n compares whose results are collected cost >= 2 n cycles of chain (fcmp -> T is 2)
    (B5) a cycle whose two slots go to an EX and an FE instruction (a T reader beside the next compare, typically) has none
                  for a load or store: with m such cycles, C >= n_LS + m

SH4ZAM's routines sit at (B2) whenever they are called in a loop; the batch routines here are written to (B1).

## 1. The routines

Cycles E: SH4ZAM or plain scalar loop -> this library, `-O3 -ffast-math` unless both levels are given (the game builds at -O2). "Bit-identical": same bits as the scalar expression / the SH4ZAM routine it replaces, on the same machine. Alignment is of the pointers; it is checked by execution (section 3) on the weakest placement allowed.

| Routine | Gap in SH4ZAM it fills | Bit-identical | Error bound | Cycles E before -> after | Alignment |
|---|---|---|---|---|---|
| `shz_dce_planes4_outcode_batch` (+ `_planes4_dist`, `_planes4_outcode`, `_planes4_set / _load`) | no plane or outcode API; XMTRX used as four plane equations | NO | beta per distance | 57 -> 18 a point (25 for the plain C form on `shz_xmtrx_transform_vec4`; 50 with `shz_vec4_dot` per plane) | planes block on 8 (it is loaded as XMTRX); points on 4, any stride that is a multiple of 4 |
| `shz_dce_planes6_cull_batch` (+ `_planes6_sphere`, `_planes6_dist`) | sphere against a convex volume of six planes | NO | beta | 89 -> 28 a sphere (plain C form: 50 at -O3, 100 at -O2) | as above; spheres x y z r on 4 |
| `shz_dce_xform_vertex32` (modes NONE / SIDES / FULL) | a vertex stream through XMTRX with clip codes, software-pipelined | YES against the plain FTRV loop | FTRV's own, nothing added | a vertex: 19 -> 13.9; with 4 side bits 36 -> 21.5; with all 7 bits 58 -> 32.5 | in on 4; out on 32 (one cache line a record, MOVCA); otherwise the plain form runs |
| `shz_dce_project_vertices` (1.8) | the whole per-vertex front end of a software renderer in one pass: FTRV, a near-first clip code against four independent slopes, the screen point by FSRRA and fused sums, vertices marked "not needed" left alone | YES against the plain loop as the compiler contracts it | FTRV's and FSRRA's own, nothing added | a vertex, -O2 / -O3: with the clip tests 80 / 86 -> 33; without 45 / 46 -> 26; behind the near plane 38 -> 24; a marked vertex 11 / 12 -> 7 | in on 4, any stride >= 24 that is a multiple of 4; view and word array on 4; out on 32 (MOVCA); otherwise the plain form runs |
| `shz_dce_vertex20_unpack` | dequantising packed vertices (16-bit grid to float) | YES for position and normal; u, v are one fused rounding | none (exact conversions, one rounding each) | 67 -> 29 a vertex (27 with out on 8, not 32) | in on 4, grid on 4, out on 32 / 8 / 4 (three paths) |
| `shz_dce_unpack3_gather` | scattered reads from a bit-packed table of index triples | YES (integer work, 32-bit copies) | none | 44 -> 24 a vertex (34 with its prefetch passes) | everything on 4; the stream is read by aligned 32-bit words only |
| `shz_dce_copy3_gather`, `shz_dce_prefetch_gather`, `_prefetch_copy3_gather` | the same for a float table; prefetch one batch ahead | YES / not a computation | none | copy 16 -> 12.6; prefetch 12 an index (a cost when every access hits) | 4 |
| `shz_dce_quat_nlerp_batch` | `shz_quat_nlerp` over arrays, one t | NO | 2^-19 absolute a component (measured 1.4e-7) | 71 -> 28 a quaternion (88 when the loop copies its operands by value) | 4; out may not overlap an input |
| `shz_dce_quat_mult_batch` | `shz_quat_mult` over arrays | NO in general (the same four FIPRs as SH4ZAM's routine: 5,818 of 5,818 results equal in the model) | (P + abs R) 2^-20 a component | 55 -> 27 | 4 |
| `shz_dce_quat_normalize_batch` | `shz_quat_normalize` over arrays | NO | 2^-19 | 31 -> 10.7 | 4 |
| `shz_dce_vec3_dot_batch` | `shz_vec3_dot` over arrays, by FIPR with pair loads | NO | beta | 14 -> 7 | inputs on 8 (FPSCR.SZ = 1 loads), out on 4 |
| `shz_dce_vec3_normalize_batch` | `shz_vec3_normalize` over arrays | NO | 2e-7 relative (measured) | 24 -> 12.75 | 4 |
| `shz_dce_vec3_dot4`, `shz_dce_vec3_cross_batch` | four dots / array cross product, plain C through pointers | YES (dot4 on the SH-4: but the sign of an exactly zero result) | as the scalar form | 61 -> 50 a quad; cross 20 -> 20 (no gain, kept as the reference) | 4 |
| `shz_dce_vec3_magnitude_fipr`, `shz_dce_vec4_normalize` (reworks) | FIPR forms of two single routines | NO: within 2 and 3 ulp of SH4ZAM's | FIPR + FSRRA | 25 -> 20 / 18 (-O2 / -O3); 31 -> 24 | registers |
| `shz_dce_sqrtf`, `_inv_sqrtf`, `_hypotf`, `_vec2/3/4_magnitude`, `_magnitude_inv`, `_distance`, `_normalize_safe` (reworks of SH4ZAM 0.10) | the zero test moved into FSRRA's shadow | YES off the zero path; on it see 2.1 | SH4ZAM's own | model: sqrtf 16 -> 14, inv_sqrtf 12 -> 11 / 9, vec3 magnitude 25 -> 23, distance 31 -> 29, normalize_safe 28 -> 25. A console: no gain, a cycle or two lost on single calls (section 7) | registers |
| `shz_dce_asinf`, `shz_dce_acosf` (reworks) | accuracy; no NaN past +-1 | NO | 8e-6 rad absolute (SH4ZAM: 4.95e-3 measured) | a console: equal in range, 57 -> 46 and 65 -> 54 over [-pi, pi] (section 7). Model: 31.7 -> 34.5 and 31.8 -> 31.5 | registers |
| `shz_dce_atan2f` (rework, 2.5) | accuracy; the number of terms is a compile-time switch | NO | e_n + 5e-7 rad: 6.2e-3, 1.4e-5, 2.5e-6, 1.0e-6 with 2, 5, 6 (default), 7 terms (SH4ZAM: 1.015e-2 measured) | model: 37.5 -> 36.5, 45.4, 46.4, 49.4. Not on a console | registers |
| `shz_dce_override.h` | macros that put SH4ZAM's names on the reworks (2.4) | | | | |

### 1.1 The error of a four-term inner product, and the bound beta

SH-4 manual (geometric instructions), normalised operands: |FIPR(a, b) - R| <= P x 2^-23 + |R| x 2^-23. The mechanism, from the bit-exact model: each product a_i b_i through a truncated Booth multiplier (FVm's lane is the multiplicand, FVn's lane is recoded; an XMTRX element takes FVn's role in FTRV), the four products aligned to the largest by truncation, summed exactly, rounded ONCE. FTRV is four such inner products, row i of XMTRX with the vector. Hence: never the bits of a scalar sum; not symmetric in its operands (3.5% of random cases); an exactly zero sum is +0; a denormal lane reads as 0.

The scalar chain s = fl(fl(fl(p_0 + p_1) + p_2) + p_3), p_i = fl(a_i b_i): to first order in u,

    |s - R| <= u (|p_0| + |p_1| + |p_2| + |p_3|) + u (|s_2| + |s_3| + |R|)
            <= u (4 P + 2 P + (P + |R|) + |R|) = (3.5 P + |R|) x 2^-23        using |s_2| <= 2 P, |s_3| = |R - p_3| <= |R| + P

and (2 P + |R|) x 2^-23 when the compiler contracts the products into FMAC. Both, and FIPR's bound, are inside

    beta(P, R) = (P + |R|) x 2^-21 = (4 P + 4 |R|) x 2^-23.

Consequence for a sign test (an outcode bit, the sign in nlerp): the computed sign can differ from the exact one only if |R| <= beta, that is |R| <= P x 2^-21 / (1 - 2^-21). For a unit normal and coordinates up to C that is a slab of half-width C x 4.8e-7 around the plane. Inside it either answer is returned; outside it the answer is exact. The tests count the differing cases and prove each is inside the slab.

### 1.2 Planes: `shz_dce_planes.h`

A plane is pi = (a, b, c, d); the signed distance of p is s(p) = a p_x + b p_y + c p_z + d = pi . (p, 1). Put four planes in the rows of a matrix M and load it as XMTRX (column-major: XF[4 c + r] = M_rc = (pi_r)_c):

    (s_0, s_1, s_2, s_3)^T = M (x, y, z, 1)^T        one FTRV
    outcode(p) = sum_i [s_i < 0] 2^i

The batch returns any = OR of the codes and all = AND (as any | all << 8): all != 0 means the batch is behind one plane, any = 0 means it is inside all four. A sphere (c, r) against six planes: planes 0..3 by FTRV, planes 4 and 5 by one FIPR each against (c, 1) (FIPR writes its result over lane 3 of its second operand, so the 1 is put back between the two: one LS slot), then

    out_i = [s_i(c) < -r] ,   cross_i = [-r <= s_i(c) <= r]

Counts a point: n_LS = 5 (three loads, fldi1, the code's store), n_FE = 4 + 4 (FTRV's h, four fcmp), (B4) = 8, so the floor is 8; the loop does two points an iteration and reaches 18 (the compare chain is not yet interleaved across points; about 12 looks reachable, not built). Planes of a special shape do NOT gain: a view-space frustum's four side planes are x +- k_x z, y +- k_y z, two multiplies for all four (26 E in a loop of its own), and the compare chain is the same length.

### 1.3 Vertex stream: `shz_dce_xform.h`

Record in: 32 bytes, floats x y z, two 32-bit words carried through, 12 bytes not read. Record out: 32 bytes on a cache line: (x', y', z') = first three lanes of XMTRX (x, y, z, 1)^T, the two words, 12 bytes left undefined (the line is allocated with MOVCA, not read). Clip code against a view volume given by slopes (k_x, k_y), guard slopes (g_x, g_y) and depths (z_n, z_f):

    bit 0: z' < z_n    bit 1: z' > z_f    bit 2: x' < -fl(k_x z')    bit 3: x' > fl(k_x z')    bit 4: y' < -fl(k_y z')    bit 5: y' > fl(k_y z')
    bit 6: |x'| > fl(g_x z') or |y'| > fl(g_y z')

Bit identity: the FTRV is the same instruction on the same operands; each compare is against one product rounded once (the negation and the absolute value are exact); the copies are integer moves. The test puts points exactly on every one of the seven boundaries and one ulp to either side of each.

Why it is faster. The plain loop is (B2): 2 (load) + 7 (FTRV's last lane) + the stores, 19 a vertex. The kernel keeps two vertices in flight in two register sets (fv4 and fv8): the three loads and the FTRV of vertex k+1 are issued BEFORE the five stores of vertex k. Then (B1) rules: n_LS = 13 (5 loads, 5 stores, MOVCA, fldi1, pref), measured 13.9. SIDES adds two fmul and four fcmp, which ride beside the loads and stores (21.5); FULL adds two more products and four more compares (32.5).

### 1.4 Packed vertices: `shz_dce_vertex.h`

Packed record, 20 bytes, read as five little-endian 32-bit words: w_0 = q_0 | q_1 << 16, w_1 = q_2 | q_3 << 16, w_2 = q_4 | n_0 << 16, w_3 = n_1 | pad << 16, w_4 = colour. q_j are unsigned 16-bit grid values, n_j signed 16-bit normal components. A batch has one grid: integer origin o in Z^3, shift s in 0..15, scales. Output record, 32 bytes (x, y, z, u, v, n_u, n_v as floats, colour):

    x  = fl( fl_int( (o_x + (q_0 << s)) mod 2^32 as int32 ) x k_p )          y, z alike with q_1, q_2
    u  = fl( q_3 k_u + b_u )   one FUSED multiply-add;      v alike with q_4
    n_u = fl( n_0 k_n ) ,  n_v = fl( n_1 k_n )

Bit identity: the int-to-float conversion is exact below 2^24 and one rounding above; one multiply is one rounding; so any implementation that converts and then multiplies gives these bits. For u and v the fused form equals fl(fl(q k) + b) whenever q k is exact, in particular for k a power of two (q < 2^16), which is the case that matters in practice. The kernel does two records an iteration: a record is 5 word loads, 7 conversions (lds fpul + float), 7 products; its floats leave in 8-byte pair stores (FPSCR.SZ = 1) and the colour as an integer store, MOVCA one record ahead. N = 51 instructions a record, so (B1) gives 25.5; measured 29 (27 without MOVCA).

Aliasing. A caller keeps these 32 bytes as a record type of its own and passes a cast pointer, then reads the result through its own type. Under strict aliasing (-O2) the compiler may take a store through one record type and a load through another as disjoint and reorder them; 4.11 measures that for SH4ZAM's own types. The asm loops are covered by their memory clobber, but the plain C path (output on a multiple of 4 only, the last record or two of every batch, the software back end) stores through the library's type. So the output record is declared `typedef struct { ... } __attribute__((may_alias)) shz_dce_vertex32_t`, with the attribute ON THE DEFINITION (anywhere else GCC drops it, 4.11): every store through it is then a store to any type. The packed input is read through may_alias 32-bit words, and the stream of 1.3 is `float *` plus may_alias words, so neither needs it. No instruction changes: same code, same E.

### 1.5 Scattered reads of a bit-packed table: `shz_dce_gather.h`

The table is a bit stream, least significant bit first, in little-endian 32-bit words: vertex v occupies bits [v W, v W + W), W = b_x + b_y + b_z <= 32, each b in 1..16, holding three indices into three axis tables. With j = floor(v W / 32) and sh = v W mod 32:

    V = (w_j >> sh) | (w_(j+1) << (32 - sh))        the second term only when sh + W > 32
    out = ( A_x[ V mod 2^b_x ] ,  A_y[ (V >> b_x) mod 2^b_y ] ,  A_z[ (V >> (b_x + b_y)) mod 2^b_z ] )

For W = 32, j = v and sh = 0: one aligned word a vertex. v < 2^27 keeps v W inside 32 bits. The figures of the table are for widths known at compile time. With widths known only at run time and 3 to 8 indices a call (the shape of a polygon's ring), both readers are slower: a byte-wise reader 157 E a vertex, the two-word path here 84 E with its prefetch passes and 62 E without (W = 31). The second word's address is j + [sh + W > 32], so no word is read that holds no bit of the vertex (shown by runs with the stream's last word at the top of the interpreter's RAM, where one more load stops the run). Exact, so bit-identical to a byte-wise reader: tested for all 3,280 (b_x, b_y, b_z). In the cache-hit model the gain is the word loads (24 against 44 E). The prefetch passes cost 10 to 12 E there and cannot be judged without a console; note that the SH-4 has one bus, so n line fills take n fill times whatever is prefetched, and a prefetch only lets up to one fill's worth of CPU work overlap.

### 1.6 Quaternion arrays: `shz_dce_quat.h`

nlerp. For unit a, b and t in [0, 1]: sigma = -1 if a . b < 0 else +1, r = (1 - t) a + t sigma b, q = r / |r|. Since |r|^2 = (1 - t)^2 + t^2 + 2 t (1 - t) |a . b| >= 1/2, FSRRA never sees a zero. Computed: d^ = FIPR(a, b); r_i by one fmul and one fmac; m^ = FIPR(r, r); f^ = FSRRA(m^); q_i^ = fl(r_i^ f^). Errors: |delta r_i| <= 2 u; m^ = |r|^2 (1 + e_m) with |e_m| <= (P + R) 2^-23 / R <= 2^-22 (a sum of squares has P <= R); f^ = |r|^-1 (1 + e_f) (1 - e_m / 2 + ...) with |e_f| <= 2^-22.37. So

    |q_i^ - q_i| <= |delta r_i| / |r| + |q_i| ( |e_m| / 2 + |e_f| + u ) <= 2 sqrt(2) u + 2^-23 + 2^-22.37 + 2^-24 = 5.3e-7 < 2^-20.8

(the header promises 2^-19; measured 1.4e-7). The sign decision uses an approximate dot product: by 1.1 it can differ from the exact one only for |a . b| <= P 2^-21 / (1 - 2^-21) <= 4.8e-7 (the header says 2^-20), where the two ways round differ in arc length by less than 1e-6 rad. The product is four FIPRs with sign-permuted operand groups (each component of a Hamilton product is a four-term inner product); the normalise is FIPR(q, q), FSRRA, four fmul.

Slots a quaternion for nlerp: n_FE = 2 FIPR + fcmp + 8 (blend) + 3 (FSRRA's h) + 4 = 18. The kernels interleave three (nlerp), one software-pipelined (product), six (normalise) elements; reached 28.2, 27.0, 10.7. The instruction ORDER inside each body was found by a search (simulated annealing) over the issue model of section 0 under the register dependences; it does not affect results, only E. A single call does not pay (88, 81, 53 E): there is nothing to overlap, and no single-call versions are shipped.

### 1.7 Vector arrays: `shz_dce_vector.h`

Dot products with pair loads. With FPSCR.SZ = 1 an `fmov` moves two floats in one LS slot. Two 12-byte elements are 24 bytes: pairs (x_0 y_0), (z_0 x_1), (y_1 z_1). The middle pair is loaded twice, so that fv4 = (x_0, y_0, z_0, x_1) and fv12 = (z_0, x_1, y_1, z_1); the foreign lanes (fr7, fr12) are zeroed, and so are the matching lanes of the second operands. Zeroing both sides matters: 0 x infinity is NaN, so a NaN or infinity in one element would otherwise poison its neighbour (found by the review tests, fixed). 14 LS slots for two elements, 7 E an element against 14 for a loop over `shz_vec3_dot`. The pointer form in plain C is no faster than SH4ZAM's loop (14): the gain is FIPR plus pair loads, not the removed struct copies.

Normalise: m^ = FIPR(v, v) with a zero lane, f^ = FSRRA(m^), three fmul. With the errors of 1.6 (here the components are exact inputs): |out_i - v_i / |v|| <= (|v_i| / |v|) (2^-23 + 2^-22.37 + 2^-24) = 3.6e-7 |v_i| / |v|; measured 1.4e-7 to 1.9e-7. Four elements an iteration, 12.75 E; the slot floor is 7, so a deeper pipeline would gain more (not built). The cross product does not gain (20 -> 20): `fmac` takes one factor from fr0 and the FPU's operations are destructive, so the loop spends 16 LS slots an element whatever is done.

### 1.8 Transform, clip code and screen point in one pass: `shz_dce_project.h`

Record in: `stride` bytes (>= 24, a multiple of 4) on a multiple of 4: floats x y z, then words w_3 w_4 w_5. Record out, 32 bytes on a cache line: (x', y', z') = first three lanes of XMTRX (x, y, z, 1)^T, then w_3, w_4, then the screen point (s_x, s_y, s_z); w_5 goes to a word array, the code to a byte array. The view is ten floats: four slopes a_x-, a_x+, a_y-, a_y+ (independent: the window need not be centred), a near depth z_n, focal length f, centre (c_x, c_y), depth map (z_b, z_s).

    code = 1                                                                                   if z' < z_n   (near first, and alone)
         = 2 [x' < fl(a_x- z')] + 4 [x' > fl(a_x+ z')] + 8 [y' < fl(a_y- z')] + 16 [y' > fl(a_y+ z')]   otherwise
    i   = FSRRA( fl(z' z') )                                    = (1 / |z'|) (1 + e) ,  |e| <= eps_f + u / 2      (SH4ZAM's shz_invf_fsrra)
    s_x = fl( c_x + fl(f x') i ) ,   s_y = fl( c_y - fl(f y') i ) ,   s_z = fl( z_b + z_s i )            each sum ONE rounding (FMAC)

The screen point is written only when the near bit is clear; with it set those 12 bytes stay as MOVCA left them (undefined). A vertex whose code byte is 0x80 on entry is not touched when the caller asks for that (a lazy transform: a pass over the faces marked the vertices no visible face uses), and the routine returns how many it processed. Without the clip tests (a batch known to be inside) every code is 0 and every vertex gets its screen point. Against the exact quotient: |s_x - (c_x + f x' / |z'|)| <= |f x' / z'| (eps_f + 1.5 u) + u |s_x|: FSRRA's error and three roundings, the plain loop's own.

Bit identity is against a compiler here, not against an expression. As written, `c + f * x * i` is ((f x) i) + c with three roundings; `-ffp-contract=fast` (the GNU C default) fuses the last product with the sum, which leaves fl(f x) and one FMAC. With `-ffast-math` the compiler may regroup first; sh-elf-gcc 15.2 does not, at -O2 or -O3. For s_y it negates either the product or i, and fl(-p) = -fl(p). The header's plain C form says exactly that (an explicit fused step; the product hidden from the optimiser by an empty asm statement, since it may regroup: 4.5), the SH-4 loop is compared with it byte for byte in four flag sets, and in the two that contract also with a loop written the plain way: 263 runs, 10,235 vertices each, no difference. The compiled loops of the project it was built for were read as well (three instances, their own flags): the same rounded products and fused sums. A build without contraction rounds twice and differs in the last bit of about a quarter of the screen coordinates (23% of 10^6 random operand triples of that size).

Slots a vertex, with the clip tests: n_LS = 27 (14 loads: the code byte, x y z, three words, seven view constants that the destructive fmul and fmac consume; 10 stores, MOVCA among them; fldi1, one register move, one prefetch), n_FE = 17 instructions and 22 cycles with FTRV's and FSRRA's holds, N = 63, so (B1) gives 31.5. (B5) decides: the four side compares are collected two cycles apart; three T readers share their cycle with the next compare, the fourth and the final shift with a fused sum, five cycles without a load or store:

    C >= n_LS + 5 + 1 (the taken loop branch) = 33 ,   reached: 33.0

one vertex at a time. Every LS slot of the vertex is in use, so a second vertex in flight would have nothing to overlap with (unlike 1.3, where the next vertex's loads fill the stores' gaps). The compiled plain loop is (B2) all the way, 80 E at -O2 and 86 at -O3: the transform's 6, then 3 + 2 in series for each side compare (the product, then T, then the branch), then 3 + 5 + 3 for z'^2, the reciprocal root and a fused sum. What the order of the kernel does against that:

- the position is loaded before the code byte has said whether the vertex is marked: its three loads fill the cycles in which the pipeline waits for that byte and its branch (a marked vertex costs 7 E, its position loaded and dropped), and FTRV issues in cycle 5;
- the carried words, the line allocation and the first slopes fill FTRV's latency. MOVCA is aimed at the address of the first carried word, so the allocation and that word's store are one instruction;
- z'^2 and FSRRA go first; FSRRA's three busy cycles are spent on the near branch and on slope loads. The side compares follow with the slope products and f y' in the gaps; x' and y' are multiplied by f in place once their own compares are behind them, which costs no register move;
- the three fused sums end the vertex, their stores beside the code's.

Without the clip tests the same loop is a serial chain, (B2): 5 (the code byte, the position, fldi1) + 6 (z') + 3 (z'^2) + 5 (FSRRA) + 2 (the third FMAC issues two cycles after the first) + 3 (its result, stored beside the loop branch) + 2 (the taken branch) = 26, against n_LS = 25; reached: 26.0 (the plain loop: 45). Two vertices in flight would reach about 24 there; not built. Behind the near plane: 24 (38).

One difference from the plain loop is visible in FPSCR only: FSRRA is issued before the near branch, so it also runs for a vertex behind the near plane, and a depth of exactly 0 sets the Z flag there (no trap: the enables are off).

## 2. Reworks of existing SH4ZAM routines, and how to switch

### 2.1 Guarded roots and lengths: `shz_dce_sqrt.h` (same bits)

Original: `shz_sqrtf(x) = (x == 0) ? 0 : FSRRA(x) * x`, and likewise `shz_inv_sqrtf`, `shz_hypotf`, `shz_vecN_magnitude`, `_magnitude_inv`, `_distance`, `_normalize_safe` (N = 2, 3, 4). Compiled: fcmp/eq, a branch that waits 2 cycles for T, then FSRRA; in order, FSRRA and everything behind it wait for that branch. Rework:

    r = FSRRA(x) * x ;   return (x == 0) ? Z : r           Z: a zero the optimiser cannot see through

The compare and the branch are issued while FSRRA runs. In the model that saves 1 to 3 cycles a call in a loop. On a console it does not (section 7): single calls are a cycle or two slower, and current SH4ZAM's `shz_sqrtf` is FSQRT, with no test to move; the baseline of this section is SH4ZAM 0.10. The reworks stay, off by default, one switch a group (2.4), with Z either a zero behind the value barrier (+0 in every build) or the plain constant (appendix B). Same signature. Results: the same operations on the same operands, so the same bits for every input off the zero path (tested by memcmp on the SH-4 in four flag sets, with zeros of both signs, denormals, squares that underflow, negatives, infinities). On the zero path there is a difference that is a finding about the original (4.4): built with `-ffast-math`, SH4ZAM's routines return their OPERAND there, not the constant 0; the reworks return +0 in every build when Z is behind the barrier, and the operand, bit for bit as SH4ZAM's routine, when it is not (both states are tested). Two side conditions: FSRRA is executed on the zero (FSRRA(+-0) = +-infinity, flag Z; then 0 x infinity sets flag V), so the divide-by-zero and invalid enables must be off, as KallistiOS has them; and where the compiler can prove x != 0 both forms compile to the same code.

### 2.2 Arcsine and arccosine: `shz_dce_trig.h` (different results: more accurate)

Original: asin(x) = atan(x FSRRA(1 - x^2)), atan(y) = y (0.97239411 - 0.19194795 y^2) for |y| <= 1 and pi/2 - that of 1/y beyond; acos = pi/2 - asin. Its error is the cubic's: 4.95e-3 rad; and for |x| > 1, which a dot product of unit vectors reaches by rounding, 1 - x^2 < 0 and the result is NaN.

Rework. acos(1 - e) = sqrt(2 e) (1 + e/12 + 3 e^2/160 + 5 e^3/896 + ...), so g(t) = acos(t) / sqrt(1 - t) is analytic on [0, 1] (g(0) = pi/2, g(1) = sqrt 2) and a short polynomial fits it; the factor sqrt(1 - t) carries the infinite slope at 1. With the constant term fixed at pi/2, so that the error vanishes at t = 0 (asin(0) = 0 exactly, asin odd and continuous across 0):

    acos(t) = sqrt(1 - t) ( pi/2 + b_1 t + b_2 t^2 + b_3 t^3 + b_4 t^4 ) + e(t) ,   0 <= t <= 1 ,   |e| <= 5.51e-6
    b = ( -0.214366547 , 0.0860212715 , -0.0382241586 , 0.0100441489 )         fitted by a Remez exchange with weight sqrt(1 - t)
    acosf(x) = x >= 0 ? acos(|x|) : pi - acos(|x|) ;   asinf(x) = sign(x) | pi/2 - acos(|x|) | ;   |x| >= 1 gives the end values

(Abramowitz and Stegun 4.4.45 is the cubic of this form with a free constant: 6.75e-5, and a jump of 1.35e-4 in asin at 0.) Written as FSRRA(1 - t) x ((1 - t) x polynomial): one FSRRA, multiplied in last. Measured against the true angle over 2 x 10^6 arguments: 5.8e-6 on the SH-4 model, 5.9e-6 in plain C; the header promises 8e-6, for every grouping of the three factors (with `-ffast-math` the compiler regroups them by call site: appendix B). Near zero the error is absolute, not relative (the original is 0.972 x there). Same signature. Cost: a console says equal in range and faster over [-pi, pi], where the range test now comes first (section 7); the model says 31.7 -> 34.5 (asin) and 31.8 -> 31.5 (acos) in range and 21 past +-1. The earlier revisions kept the product behind the value barrier, for which the model gives 28.5; the console showed no gain from it in range and a loss past +-1 (46 -> 65 cycles), so it is now a switch, off (`SHZ_DCE_OPAQUE_ASIN_ACOS`). The same two routines on SH4ZAM alone, for upstream: `doc/upstream/shz_asin_acos.h`. A consequence inside SH4ZAM: `shz_quat_slerp` takes its angle from `shz_acosf`; with the original's error the result's length is off by up to 1.9e-3 and its angle along the arc by 3.4e-4 rad (computed from the formula); with this arccosine, by about 2e-6 and 4e-7.

### 2.3 FIPR forms of two single routines: `shz_dce_vector.h` (within 2 and 3 ulp)

`shz_dce_vec3_magnitude_fipr`: the squared length by one FIPR (latency 4 against fmul + 2 fmac = 9), and the zero test in FSRRA's shadow: 25 -> 20 / 18 E. `shz_dce_vec4_normalize`: FIPR over four lanes (w saved first, FIPR overwrites its lane): 31 -> 24 E. Other single calls by FIPR were measured and NOT shipped, because moving the operands into fv registers eats the gain: normalise 24 -> 22, safe normalise 27 -> 25, distance 31 -> 27.

### 2.4 Switching: `shz_dce_override.h`

    #include <sh4zam_dce/shz_dce_override.h>      /* includes SH4ZAM first */

defines SH4ZAM's names as macros for the reworks, one compile-time switch a group, each 1 or 0, set before the include: `SHZ_DCE_USE_ASIN_ACOS` (default 1: `shz_asinf`, `shz_acosf`), and, default 0, `SHZ_DCE_USE_SQRT` (`shz_sqrtf`, `shz_hypotf`), `_INV_SQRT`, `_MAGNITUDE` (lengths and their reciprocals), `_DISTANCE`, `_NORMALIZE` (the safe normalisations), `_FIPR` (`shz_vec4_normalize`, and `shz_vec3_magnitude` becomes the FIPR form), `_ATAN2` (`shz_atan2f`, 2.5); `SHZ_DCE_OVERRIDE_ALL 1` turns on every one not set by name. The defaults are the console's verdict (section 7): the two that won are on, the rest wait for measurements in a real program. A second set, `SHZ_DCE_OPAQUE_<group>`, says where a rework puts the value barrier (appendix B); its defaults are the console's too. `#undef` a name to take one routine back. The macros act on code that follows them: SH4ZAM's own inline routines were compiled before and keep calling the originals, which is why the dependants (lengths, distances, safe normalisations) have reworks of their own. A project-level convenience; upstream the change belongs in the routines themselves (section 5).

### 2.5 The angle of a vector: `shz_dce_atan2.h` (different results: more accurate, slower)

Original (`shz_atan2f`). With s = |y| it forms one quotient and one arctangent:

    x >  0:   r = (x - s) / (x + s) ,   angle = pi/4  - atan(r)
    x <= 0:   r = (x + s) / (s - x) ,   angle = 3pi/4 - atan(r)          then the sign of y; (0, 0) gives 0

Both lines are tan(pi/4 - a) = (1 - tan a) / (1 + tan a): exact identities, r in [-1, 1], the axes at r = +-1. The quotient is num x FSRRA(den^2).

Where the error comes from. Against atan2 in double over 4 x 10^6 points the worst error is 1.015e-2 rad, and all of it is the arctangent, taken as 0.9817 r - 0.1963 r^3 (the constants of a widely copied fast atan2, quoted there with 0.01 rad). The quotient cannot give more than 2.2e-7: FSRRA's 2^-22.37 and four roundings, times d atan / d ln r = |r| / (1 + r^2) <= 1/2. The cubic has the right shape: 0.9817 - 0.1963 = 0.7854, pi/4 to 2e-6, so it has no error at r = +-1, and the axes come out right. But among the cubics a r + (pi/4 - a) r^3 it is not the best: a = 0.967482567 gives 6.131e-3, 0.9817 gives 1.015e-2 (at r = 0.785). So 40% of the error is the choice of a (4.12). Without the condition at 1 the minimax cubic is 0.972394109 r - 0.191947952 r^3, the one in `shz_atanf_unit`: 4.952e-3, but its worst points are r = +-1 themselves, and atan2(0, 1) would be 4.95e-3 instead of 0. For an angle that feeds a camera the axes matter more than the last 20%.

Rework. The same quotient; atan(r) = r P(r^2) with P of n terms, n = 2, 5, 6 or 7, fitted by a Remez exchange on [-1, 1] to the absolute error of r P(r^2) under the original's condition P(1) = pi/4. The condition costs a tenth of the accuracy (free fits of 5, 6, 7 terms: 1.144e-5, 1.66e-6, 2.47e-7) and keeps the axes within 6e-7 for every n (tested). P is written in pairs (Estrin): after u = r^2 and u^2 the pairs do not wait for each other.

    | shz_dce_atan2f(y, x) - atan2(y, x) |  <=  e_n + 5e-7 ,     e_2 = 6.131e-3   e_5 = 1.250e-5   e_6 = 1.803e-6   e_7 = 2.70e-7
    for finite x, y with 2e-18 <= |x| + |y| <= 5e17 ;  the header states 6.2e-3, 1.4e-5, 2.5e-6, 1.0e-6

e_n is the fit's error with the constants as floats. The 5e-7 is a budget, not a proof: 2.2e-7 for the quotient, 1.0e-7 for the polynomial's own roundings, 1.5e-7 for the rounding of a result of magnitude up to pi and of the two base constants. The tests have not exceeded it (worst found minus e_n: 2.7e-7 at most). The range is that of 4.9: the denominator is squared.

    form                                         worst error found, rad       cycles E     fmac + fmul an element
    shz_atan2f                                   1.015e-2                     37.45        2 + 3
    2 terms: its code, the best constant         6.14e-3                      36.45        2 + 3
    5 terms                                      1.28e-5                      45.42        5 + 4
    6 terms (the default)                        2.07e-6                      46.42        6 + 4
    7 terms                                      5.3e-7                       49.42        7 + 4
    octant reduction, 3 terms (not shipped)      3.8e-6                       56.34
    octant reduction, 4 terms (not shipped)      4.0e-7                       61.28

Errors: the worst over 4 x 10^6 points on the host (SH4ZAM's software back end) and 9 x 10^4 on the SH-4 model with FSRRA's own bits, in four flag sets. Cycles: a loop over arrays of x and y uniform in [-1, 1), -O2 and -O3 alike, fast-math flags; branches are averages over that distribution.

The octant forms reduce a second time (mx, mn the larger and smaller of |x|, |y|; r = (mn - t mx) / (mx + t mn) with t = tan(pi/8), so |r| <= t) and need only three or four terms. They lose by 10 and 12 cycles against the quadrant forms of about the same accuracy (56.3 against 46.4, 61.3 against 49.4): the second reduction is a compare, two selects and two fused sums before FSRRA and two conditional reflections after it, and the terms it saves are cheap (a term is 1 to 3 cycles in the quadrant forms). On this machine a multiply-add costs one FE slot and overlaps with its neighbours; a compare costs two cycles to the T bit and a branch. Range reduction by table (FSCA has no inverse) was not tried: a load whose address depends on the argument is a cache miss the model cannot see.

So: accuracy is bought with cycles here, about 9 E for a factor of 5000, and nothing faster than the original came out of C. The 2-term form is the original's code with another constant (the one cycle is a matter of instruction order and may not show on a console). A faster atan2 would need an assembly block. Of the original's 37 cycles 14 are the quotient (add, square, FSRRA, multiply), and it executes three compares and two taken branches an element, in the order written. With the sign and the half plane taken on the integer side, and the products that do not need FSRRA's result computed while it runs, my estimate is about 28 E for a cubic. Not built; and the model has been wrong about single calls with a branch behind FSRRA (section 7, Q6), so I do not promise it.

Special cases as the original: (+-0, +-0) gives 0; the result has the sign of y, a zero y of either sign counting as positive (y = -0, x < 0 gives +pi; C's atan2 gives -pi, the same direction). Same signature. `SHZ_DCE_ATAN2_TERMS` (2, 5, 6, 7; default 6) picks the form before the include; `SHZ_DCE_USE_ATAN2` (2.4), off by default, puts SH4ZAM's name on it. Off because it is not on a console yet and costs cycles in the model: a caller that feeds a camera turns it on, a caller that sorts by angle does not need it.

## 3. Method

The interpreter (`tests/sh4run`, about 1,000 lines of C, 1,600 with its tools and self-test) loads an ELF32 little-endian SH-4 executable and runs one function at a time on a 16 MB RAM image. It is strict where an emulator is lenient:

- every 16 / 32 / 64-bit access must be on a multiple of 2 / 4 / 8, and inside RAM: otherwise the run stops (on the console: an address error). A pass is therefore a proof of alignment for every instruction executed, and the tests place each buffer on the weakest multiple a routine allows (for instance on 4 mod 8).
- at return, r8-r14, r15, fr12-fr15, GBR and the FPSCR mode bits (FR, SZ, PR, DN, RM) must be as they were at entry.
- `movca.l` fills the other 28 bytes of its line with a poison pattern: a kernel that allocates a line it does not completely write, or that a neighbour lives in, fails its compare.
- scratch registers start as garbage, NaNs on alternate calls; every job is run twice and must give the same bytes.
- an instruction that is not modelled stops the run and names itself (no silent no-ops).
- FIPR, FTRV, FSRRA and FSCA give the console's bits through a third party's bit-exact model: `sh4_fpu_approx.h`, MIT, (c) 2026 Stefanos Kornilios Mitsis Poiitidis (skmp), each instruction verified by its author against a Dreamcast with 0 mismatches (FSRRA on every positive normal input, 2,130,706,432 of them, and on the special operands with their FPSCR effects). It is used in the host-side tests only, kept with its own notice. Without it the interpreter falls back to exact sums rounded once. FMAC is modelled as one fused operation (Q2). A NaN result of FADD, FSUB, FMUL, FDIV, FMAC or FSQRT is the SH-4's one quiet NaN, H'7FBFFFFF (the manual's qnan()): the host would hand on an operand's bits, which one depending on the operand order the compiler chose, and two correct builds of one routine then differed in NaN bits only.
- timing: the model of section 0. It knows no cache miss, interrupt, TLB or store queue, and has not been compared with a console.

What supports the interpreter itself: (1) its decoder against GNU objdump on every 16-bit word of the code of every test executable: 145,569 words in 45 ELFs, 0 differences; (2) a self-test that compiles the same C functions natively and for the SH-4 and compares results (integer arithmetic including division steps and shifts, single and double arithmetic, fmac, conversions, argument passing), plus planted faults that must each stop a run: unaligned 16-, 32- and 64-bit accesses, an access outside RAM, r8, fr12, FPSCR.SZ and the stack pointer not restored; and a line allocated by `movca.l` and half written must show the poison in the other half.

Per routine there are four checks, run by one driver (`python tests/run_checks.py [--only topic] [selftest host compile sh4 cycles]`): a host test on SH4ZAM's software back end against an exact reference in double or integers, built with strict flags and with `-ffast-math`; an SH-4 compile of every header alone and of the kernels with `-Wall -Wextra -Werror` (148 compiles); the SH-4 kernels executed in the interpreter in four flag sets (-O2 / -O3, strict / fast-math), comparing the asm form with the plain C form bit for bit where both use the same FTRV / FIPR, both with the exact value inside the stated bound, guard bytes, XMTRX unchanged; and the cycle table. Counts in the last full run, all passing:

    host tests (each flag set):  planes 596,358   xform 279,971   vertex 4,110,880   gather 1,006,541   quat 263,638   vector 43,081
                                 sqrt 7,159,352   trig 12,800,006   override 60,024 + 160,021   project 164,602   upstream 12,800,009   atan2 36,000,043
    SH-4 runs (each of 4 sets):  planes 23,938   xform 38,256   vertex 440,822   gather 16,352   quat 76,125   vector 94,236
                                 sqrt 86,400   trig 1,479,157   project 316,091 (441,893 in the two sets that contract)   upstream 458,941   atan2 1,081,617

A fused multiply-add in a reference is never the host C library's `fmaf`. The one of the host compiler used here (x86_64-w64-mingw32, GCC 16.2) returns a neighbour of the correctly rounded a b + c in 241,553 of 10^6 random cases, on either side (50,954 of them the twice-rounded value); its `fma` in double, rounded to float, agrees in all 10^6. The interpreter and the SH-4 checks are built with the FMA instruction (`-mfma`), and the library's software form (1.4) is tested against integers.

Four of the topics (vertex, gather, quaternion, vector) also went through a second, adversarial pass whose brief was to break the header's claims; it found, among smaller things, the NaN leak of 1.7 and a loop that ran away on a count of zero, both fixed before the numbers above.

## 4. Findings about SH4ZAM itself

Loop costs are E for `out[i] = f(a[i], ...)` over arrays, -O3 with the fast-math flags unless stated. Appendix A has the full table.

4.1 Which routines are approximate is not uniform. `SHZ_SH4_DOT_FIPR` is off, so `shz_vec2/3/4_dot`, `shz_dot6f / 8f`, `shz_mag_sqr3f / 4f` and everything built on them (magnitudes, normalise, distance, `shz_quat_dot`) are fmul + fmac chains: exact in the sense of 1.1's scalar chain. But `shz_quat_mult`, `shz_quat_transform_vec3`, `shz_mat3x3_transform_vec3`, `shz_mat4x4_transform_vec3 / vec4` (and their transposes), `shz_vecN_dot2 / dot3` and every XMTRX transform use FIPR or FTRV regardless of that switch: approximate, one rounding, not symmetric in their operands. A user who needs reproducible bits cannot tell from the documentation which routine is which. Also: in a strict-IEEE build `shz_dot6f` (through `shz_dot8f` with zero lanes) keeps the added 0 x 0 term, because dropping it changes the sign of a -0 sum: three fadd for three products, 21 E against 14 with fast-math.

4.2 By-value unions go through the stack. GCC does not scalarise `shz_vec3_t` / `shz_vec4_t` / `shz_quat_t` (unions inside): a loop over them copies each operand to the stack with integer moves and reloads it. Copying a vec3 array through a by-value local: 10 E, 3 loads and 6 stores an element. `shz_quat_nlerp` in a loop: 71 to 88 E (20 loads, 12 to 20 stores) for arithmetic that needs 18 FE slots; `shz_quat_mult`: 55 to 65 E for four FIPRs. Batch routines that take pointers and read floats where they sit: 28 and 27 E.

4.3 (Withdrawn as a finding; kept as a record.) In the model a zero test in front of FSRRA costs 1 to 4 cycles a call (2.1): `shz_sqrtf` of SH4ZAM 0.10 16 E against 12 to 13 for `shz_sqrtf_fsrra`; the same in `shz_inv_sqrtf`, the magnitudes, distances, `_normalize_safe`, `shz_hypotf`. The reorder keeps every bit. A console does not show the gain on single calls (section 7), so this is a statement about the model, not about the SH-4.

4.4 Under `-ffast-math` the zero path returns the operand. `(x == 0.0f) ? 0.0f : ...` lets the compiler substitute x for the constant (no signed zeros), and it does (-O2 and -O3): `shz_sqrtf(-0.0f)` is -0.0f, and a denormal, which compares equal to zero with FPSCR.DN = 1, comes back as that denormal (1,500 such results counted in one test run). Equal as numbers on the SH-4; different as bits, and different from the documented 0.

4.5 The compiler's order, and what fast-math does to a formula. Three observations that limit what a C-level routine can do on this machine. (a) GCC emits the multiply that consumes FSRRA's result directly behind it and queues independent work (a polynomial, the other operand's arithmetic) after that multiply, which then waits out FSRRA's latency with everything behind it: (B2) instead of (B3). (b) With `-ffast-math` it regroups (pass reassoc, by its own rank of each value): inside a loop over an array `inv * (a * p)` comes back as `(inv * a) * p`, in a standalone function the reciprocal root is moved last instead. (c) An empty asm statement on a value (`asm("" : "+f"(v))`, no instruction) makes it opaque and stops (b) and 4.4's substitution at that value; it also keeps what feeds it in front of a following branch, which costs the side that did not need it (appendix B has all three, from the compiler's dumps). Only an asm block fixes an order. Example: `shz_atan2f` (37.4 E). Its dependence chain is fadd, fmul, FSRRA, fmul (14 cycles to the quotient q), then q^2, fmac, fmul, fadd (12 more). Evaluating c_1 d^2 and c_0 d (d the numerator) while FSRRA runs would shorten the tail to about 9; rewritten in C with one quotient and no reflection subtract it measures 36.5 E, because the emitted order is the same. Not reworked.

4.6 Accuracy of the inverse trigonometric routines, measured against libm in double: `shz_atanf` on [-1, 1]: 4.95e-3 rad (0.28 degrees); `shz_asinf`, `shz_acosf`: 4.95e-3 rad, NaN for |x| > 1; `shz_atan2f`: 1.015e-2 rad (0.58 degrees), and atan2(0, x > 0) = 1.84e-6 instead of 0 (pi/4 - (0.9817 - 0.1963) is not zero). Documentation: `shz_atanf_q1` is described as valid "within the first quadrant (>= 0.0f)", but it computes pi/2 - atan_unit(1 / x), which is only right for x >= 1: `shz_atanf_q1(0.5f)` = 1.1616 against atan(0.5) = 0.4636. `shz_atanf` only calls it beyond 1, so nothing breaks inside the library. `shz_sincosu16`'s comment says the 16-bit angle is "in radians"; it is 1/65536 of a turn.

4.7 `shz_mat3x3_transform_vec3` in a loop reloads the matrix for every vector: 18 loads and 3 FIPRs, 31 E a vector; `shz_mat4x4_transform_vec4` 37 E. The same through XMTRX is 13 to 15 E in a plain loop, and the pipelined kernel of 1.3 moves a whole 32-byte record (three floats transformed, two words carried, a line allocated) in 13.9. The documentation says to batch through XMTRX; a routine that does it would remove the temptation.

4.8 Small things in the SH-4 back end. `shz_dcache_alloc_line_sh4` executes `movca.l r0, @ptr` without naming r0 as an input: the first word of the line receives whatever r0 holds (harmless only when the caller overwrites that word, which the function's contract does not say), and the register allocator is free to put `ptr` itself in r0. `SHZ_PREFETCH` has no memory clobber, so the compiler may move a load across it (timing only). Compile-time paths (`__builtin_constant_p`) return libm values where the run-time path returns the approximation: `shz_sqrtf(2.0f)` and `shz_sqrtf(x)` with x = 2 at run time differ in the last bit or two, and which one a call site gets depends on inlining and constant propagation; worth a sentence for users who compare results across builds.

4.9 Domains under DN = 1. `shz_invf_fsrra(x) = FSRRA(x^2)` and everything on it (`shz_divf`, `shz_atanf_q1`, `shz_atan2f`) need 1.1e-19 < |x| < 1.8e19 (x^2 in the normal range). The normalise routines lose a component whose square falls below 2^-126: `shz_quat_normalize` of a quaternion of length 1e-17 with components in the ratio 1 : 0.01 is 1e-4 off; FIPR, by the model, keeps its products internally and is 6e-8 off at 1e-18. Not a defect, a documented range would help.

4.10 Routines measured and found at the machine's floor for a single call (no rework possible in C, none attempted in asm): `shz_sincosf` / `shz_sinf` (17 / 16 E: one fmul, ftrc, FSCA), `shz_tanf` (28; the sign-free sin cos FSRRA(cos^2)^2 is 32), `shz_invf` (14.5), `shz_divf` (16.5), `shz_lerpf` (11), `shz_clampf` / `shz_fminf` / `shz_fmaxf` (12.5 / 9.5: one T bit, two cycles a compare), `shz_atanf` (17), `shz_vec3_cross` (20, see 1.7).

4.11 `shz_vecN_deref` does not grant the aliasing it names. The macros are `(*((SHZ_ALIASING shz_vec3_t*)(ptr)))`: a may_alias attribute written in a cast. GCC 15.2 drops it without a diagnostic there, and on a typedef of the finished type it says so ("ignoring attributes applied to 'struct shz_vec3' after definition"). What remains is a plain cast to a union type, and GCC takes an access through that union and an access through another record type as disjoint, even when both reach a float at offset 0. Compiled for the SH-4 at -O2, with `typedef struct { float x, y, z; } mine_t`:

    float f(mine_t *m, void *p) { m->x = 1.0f; shz_vec3_deref(p).x = 2.0f; return m->x; }      returns the constant 1.0f, no reload
    float g(mine_t *m, void *p) { shz_vec3_deref(p).x = 2.0f; m->x = 1.0f; return shz_vec3_deref(p).x; }   returns the constant 2.0f

Both are wrong when p == m, which is the case the macro exists for. The same two functions with a `float *` in place of `mine_t *` reload correctly (a float lvalue conflicts with the union), and so does a record type that carries may_alias on its own definition. So `shz_vecN_deref` is safe on float arrays and unsafe on a caller's struct; nothing warns. A caller is exposed whenever it derefs its own vertex or transform struct, writes through one view and reads through the other in the same function after inlining.

4.12 `shz_atan2f` takes its arctangent as 0.9817 r - 0.1963 r^3: 1.015e-2 rad at worst, at r = 0.785 (2.5). The two constants add up to pi/4, which keeps the axes exact and is worth keeping; under that condition the best cubic is 0.967482567 r - 0.182084382 r^3, 6.131e-3 rad, in the same instructions. (The unconditioned minimax cubic of `shz_atanf_unit` would give 4.952e-3 and put all of it on the axes.)

## 5. Suggestions for upstream

Worth taking, in the order I would take them:

1. (Withdrawn after section 7.) The guarded-root reorder (2.1) does not pay on a console, and current SH4ZAM's `shz_sqrtf` no longer has the test. What remains of it is a question, not a suggestion: what the zero path should return under fast-math (4.4). A zero behind the value barrier gives +0 in every build; on the console it helped `inv_sqrtf` and hurt four roots in a row.
2. The arccosine of 2.2 (and asin on it): 850 times closer to the true angle for the same cycles or fewer on a console, no NaN at 1 + ulp, and `shz_quat_slerp` improves with it. Ready to copy: `doc/upstream/shz_asin_acos.h`, on SH4ZAM alone, without the barrier.
3. A statement per routine in the documentation: exact chain, or FIPR / FTRV / FSRRA with its bound. The measured constants of section 0 (FSRRA 2^-22.37 relative, FSCA 2^-23.09 absolute, FIPR 0.42 of the manual's bound) could stand next to `SHZ_FSCA_ERROR_MAX`.
4. Batch API where the pipeline pays by a factor of two or more. From this library: the vertex stream through XMTRX with optional clip codes (1.3; a version with strides instead of fixed 32-byte records is a small change), plane sets (1.2), quaternion nlerp / normalise / product over arrays (1.6), vec3 dot and normalise over arrays (1.7), the dequantising unpack (1.4, with the grid struct as the only format assumption). All of it is in the sister include with its tests; take the idea or the code.
5. The interpreter as a check for SH4ZAM itself: alignment and ABI by execution, the movca poison, and a cycle estimate for every routine on every change. It needs only a host compiler and sh-elf-gcc.
6. Pointer-taking variants (or struct types the compiler can scalarise) for the quaternion and vector routines used in loops (4.2).
7. The two asm declarations of 4.8, and the two documentation lines of 4.6.
8. `SHZ_ALIASING` on the DEFINITIONS of `shz_vec2_t`, `shz_vec3_t`, `shz_vec4_t`, `shz_quat_t` and the matrix types (4.11), or a sentence that `shz_vecN_deref` is for float arrays only. The attribute on the definition costs the optimiser its type-based disambiguation for these types and nothing else; the deref macros then mean what they say.

9. `shz_atan2f` (2.5, 4.12). For no cycles: 0.967482567 and 0.182084382 in place of 0.9817 and 0.1963 (1.015e-2 -> 6.13e-3, the axes as exact as now). For accuracy: the 6-term form (2.5e-6, about 9 E more in the model), as a second routine or behind a compile-time choice, since some callers want the cheap one. The 5-term form saves one cycle of the nine and loses a factor of 6; I would not keep it.

What I would NOT take: `shz_dce_vec3_cross_batch` and `shz_dce_vec3_dot4` (no or small gain; they are references for the tests); single-call FIPR forms other than the two of 2.3 (under 15%); the prefetch passes of the gather routine and the MOVCA-ahead of the unpack until a console shows they pay; the searched instruction orders of 1.6 until a console confirms the model's latencies (the code is correct either way, only the E is at stake); the override macros (they are for a project that cannot edit the library); anything in this include that claims "YES" for bit identity without your own check of the FMAC and pair-move questions below.

## 6. Open questions

Q1. FSRRA and FSCA timing. The model uses FSRRA latency 5 with the FE stage held 3 cycles, FSCA 6 / 7 held 4, FIPR 4, FTRV 5 to 7 held 4, fcmp to T 2. The first two are assumptions (the SH7750 manual predates them). Do you have measured figures, and does a second FE instruction issue during FSRRA's execution as assumed?

Q2. FMAC. Is it one fused operation with a single rounding on the SH7091 (the manual's operation description reads that way)? The u, v claim of 1.4 for scales that are not powers of two, and every "bit-identical under -ffp-contract=fast" statement about contracted code, depend on it.

Q3. Pair moves. With FPSCR.SZ = 1 and little-endian memory, `fmov @Rm, DRn` puts the float at the lower address into FRn (the even register). Your XMTRX loads rely on the same order; confirmed on hardware for XF and FR alike? The dot batch (1.7) and the pair stores of the unpack (1.4) depend on it, and the tests cannot see a reversed order (they share the model).

Q4. Policy. `SHZ_SH4_DOT_FIPR` is off by default while other routines use FIPR unconditionally (4.1). Is the intent "scalar-exact unless the name says otherwise"? If so, would `_fipr`-suffixed single routines and array routines that are approximate by contract fit the library, and would you want the zero path of the guarded routines pinned to +0? And for 4.11: are the public types meant to be viewed over a caller's own structs (then they need may_alias on their definitions), or only over float arrays?

Q5. Schedules. Would you accept kernels whose instruction order comes from a search over a pipeline model, with the model's parameters stated, or only orders measured on a console? And two hardware details the model guesses: is `movca.l` on a line that is already in the cache a plain store, and does a taken `bf/s` cost what a taken `bf` costs (the model charges 2 cycles to both and counts the slot as free work)?

Q6. Where the model is wrong (after section 7). For `r = FSRRA(x) * x; if (x == 0) r = 0` against `x == 0 ? 0 : FSRRA(x) * x` the model predicts 2 cycles less a call, a loop or a single call; your console shows 1 to 2 more on single calls. Candidates I cannot tell apart from here: FSRRA holds more than the FE stage (a conditional branch behind it waits for it); fcmp to T to branch costs less than 2 + 1 when nothing else is in flight; or your harness's call and return dominate. A loop over an array on the console (64 and 192 elements, the difference divided by 128, as `sh4bench` does) would say whether the batch figures of section 1 share the error.

Q7. `shz_atan2f` (2.5). Which do you want: the same cycles and 0.6 of the error (two constants), or 2.5e-6 for what the model says is 9 cycles? The four forms are one macro apart (`SHZ_DCE_ATAN2_TERMS` 2, 5, 6, 7; `shz_dce_atan2f` beside `shz_atan2f`), so one run of your harness gives the console's price for each. If the console's price for the long forms is well under the model's 9 (the multiply-adds overlap, and your single-call figures have been kinder to straight-line code than the model), the default switch should go on.

## 7. A console's results (reported by Falco Girgis, 2026-11-18)

Single calls on a Dreamcast, cycles cached / uncached. "SH4ZAM": the library as it is today (its `shz_sqrtf` is FSQRT, where 0.10 had the zero test and FSRRA). "Rework": the routine of 2.1 / 2.2 without the value barrier. "+ barrier": the same with the barrier back (appendix B). The build flags were not reported. Bold is the best of a row.

| Routine | SH4ZAM | Rework | Rework + barrier |
|---|---|---|---|
| inv_sqrtf | 25 / 100 | 26 / 88 | **24 / 84** |
| sqrtf | **28 / 74** | 29 / 102 | 29 / 98 |
| sqrtf, four in a row | 65 / **126** | **59** / 138 | 66 / 156 |
| asinf, abs x < 1 | 63 / 228 | 64 / **208** | 65 / 210 |
| acosf, abs x < 1 | 65 / 288 | 64 / **252** | 65 / 260 |
| asinf, x over [-pi, pi] | 57 / 254 | **46 / 202** | 65 / 210 |
| acosf, x over [-pi, pi] | 65 / 288 | **54 / 256** | 68 / 270 |
| vec3 normalize_safe | **52 / 134** | 60 / 212 | 63 / 220 |
| vec4 normalize_safe | **50 / 188** | 65 / 250 | 66 / 260 |
| vec2 normalize_safe | 44 / 136 | **40** / 138 | **40 / 133** |
| csqrtf | **65 / 121** | 81 / 178 | 75 / 166 |

Also reported, without rows: single calls of the reordered roots, lengths and distances are 1 to 2 cycles slower cached, the vec3 / vec4 distances 2 to 4; `casinf` + 19 and `cacosf` + 21 with the reworks inside SH4ZAM. Four builds were run: baseline, all the C-level reworks, the root rework alone, and SH4ZAM's own FIPR path (`SHZ_SH4_DOT_FIPR`) standing in for the FIPR length and normalise of 2.3.

What it says, and what this library did with it:

1. The arcsine and arccosine win: 850 times closer to the true angle, the same cycles in range, fewer over [-pi, pi]. They are on by default in the override header and offered upstream as one file.
2. The zero-test reorder loses on single calls. The model had it winning by 2 cycles; it was measured against SH4ZAM 0.10, in loops, and by a model whose FSRRA timing is an assumption (Q1, Q6). Every figure of 2.1 is therefore the model's alone, the reworks are off by default, and suggestion 1 is withdrawn. One row agrees with the model in direction: four independent roots in a row, 65 -> 59 cached (model: 43 -> 35 an element).
3. The barrier is not free and not always wrong: it wins on `inv_sqrtf` and the vec2 normalisation, loses on four roots in a row and on vec3 / vec4, and in the arcsine and arccosine it moves the polynomial in front of the range test, which costs 19 and 14 cycles for arguments past +-1 (the model: 9 and 8) and gains nothing in range (the model had promised 3 to 6). Its switches default to this table.
4. What the table does not cover: the batch routines of section 1 (their gain is overlap between elements, which a single call does not have) and anything in a loop. Their E stay estimates.



## Appendix A. SH4ZAM routines in a loop (cycles E, instructions, loads + stores an element; -O3, fast-math flags)

    sqrtf 16 (10; 1+1)            sqrtf_fsrra 13 (8)          inv_sqrtf 12 (8)            inv_sqrtf_fsrra 10 (6)      invf 14.5 (10.5)
    invf_fsrra 13 (7)             divf 16.5 (12.5; 2+1)       divf_fsrra 17 (9)           x / y (fdiv) 17 (7)         lerpf 11 (8)
    fmaf 8 (8)                    smoothstepf 19.1 (14.7)     smoothstepf_safe 24.6       clampf 12.5 (9.5)           saturatef 12.5
    floorf 15.5 (9.5)             ceilf 15.5                  roundf 25.5 (18.9)          fractf 17.5                 signf 10.5
    copysignf 8.5                 fminf 9.5                   fmodf 32 (21)               hypotf 23 (14)              cbrtf 66 (47)
    powf 27.9                     expf 21                     logf 14                     sinf 16 (8)                 cosf 17
    sincosf 17 (11; 1+2)          tanf 28 (14)                atanf 17 (14)               atan2f 37.4 (30.4)          asinf 31.7 (23)
    acosf 31.8 (24.3)             vec3_normalize 24 (19; 3+3) vec3_normalize_safe 27      vec3_magnitude 25 (17)      vec3_magnitude_inv 21
    vec3_magnitude_sqr 13         vec3_distance 31 (23; 6+1)  vec3_distance_sqr 19        vec3_dot 14 (14; 6+1)       vec3_cross 20 (25; 6+3)
    vec3_lerp 21 (19; 6+3)        vec3_reflect 22             vec3_project 39             vec3_add 14                 vec3_angle_between 82
    vec3_dot3 25 (27; 15+3)       vec4_dot 17 (17; 8+1)       vec4_dot2 26                vec4_normalize 31 (25; 4+4) quat_normalize 31
    quat_mult 55 to 65 (65 to 83) quat_nlerp 71 to 88         quat_lerp 49                quat_slerp 143              quat_transform_vec3 45
    mat3x3_transform_vec3 31 (40; 18+6)                       mat4x4_transform_vec3 27    mat4x4_transform_vec4 37    mat4x4_transform_point3 36
    xmtrx_transform_vec4 15 (12; 4+4)                         xmtrx_transform_point3 13   xmtrx_transform_vec3 13     copy of a vec3 by value 10 (16; 3+6)

Ranges are two ways of writing the same loop (operands copied to locals, or passed straight from the arrays). A figure is the slope between 64 and 192 elements; inputs are uniform in [-1, 1), so data-dependent branches (clamp, round, atan2) are averages over that distribution.

## Appendix B. The value barrier `shz_dce_opaquef()`

Asked for by Falco Girgis after section 7's measurements ("explain exactly what it does"). "Your table" below is section 7's.

SH4ZAM-DCE, 2026-11-18. Everything quoted below is the compiler's own output: sh-elf-gcc 15.2.0, `-ml -m4-single -O2 -ffast-math -ffp-contract=fast -fno-math-errno -mfsrra -mfsca` ("fast"). `-O3` gives the same in every case shown. "Strict" is `-fno-fast-math -ffp-contract=off`. Cycle figures marked E are estimates from my pipeline model, not a console; where your table has the row, your table is the truth.

### B.0 Short version

It is one extended asm statement with an empty template: no instruction is emitted. What it does is make GCC forget what a value is. After it, the value is "whatever that statement left in an FP register": not a constant, not equal to any other variable, not the result of a multiply. Three optimisations stop there, and each one moves rows of your table:

1. The swap of a constant for an operand that compared equal to it (pass `uncprop1`, allowed by `-fno-signed-zeros`). Without the barrier a zero path returns its ARGUMENT; with it, a real +0.
2. The regrouping of a product (pass `reassoc1`, `-fassociative-math`).
3. Sinking (pass `sink1`). GCC 15.2 does not move an asm statement below a later branch, so everything it consumes is computed before that branch. That is why asinf / acosf "x in +-pi" went from 46 / 54 to 65 / 68 cycles when the barrier went back in: the polynomial is then computed for arguments past 1 too.

It is not a memory barrier, it is not `volatile`, and it does not fix the order of instructions.

### B.1 The statement

    SHZ_FORCE_INLINE float shz_dce_opaquef(float value) {
    #if SHZ_DCE_SH4
        __asm__("" : "+f" (value));
    #endif
        return value;
    }

`f` is the SH constraint letter for "any FP register", `+` says the operand is read and written, the template is empty, so the assembler receives nothing. Inside GCC (GIMPLE) the statement is

    __asm__("" : "=f" value_96 : "0" _95);

an output `value_96` in an FP register and an input `_95` that must sit in the same register. The manual (Extended Asm): "GCC does not parse the assembler instructions themselves and does not know what they mean". All any pass knows about an asm output is that this statement defines it, so `value_96` is a new value with no visible relation to `_95`. (The manual says nothing about an empty template; the dumps show it is treated like any other.) Your `shz_opaquef` is the same statement. Everything below follows from that, plus one more fact: it is a statement, not an expression (B.2.3).

### B.2 What stops at it

#### B.2.1 What the compiler knows about the value: the zero path

    float plain  (float x) { const float r = shz_inv_sqrtf_fsrra(x) * x; return x == 0.0f ? 0.0f : r; }
    float barrier(float x) { const float r = shz_inv_sqrtf_fsrra(x) * x; return x == 0.0f ? shz_dce_opaquef(0.0f) : r; }
    float shz    (float x) { return shz_sqrtf(x); }      /* SH4ZAM 0.10.0: (x == 0) ? 0 : FSRRA(x) * x */

    plain:                      barrier:                    shz (0.10.0):
        fldi0   fr2                 fldi0   fr0                 fldi0   fr1
        fcmp/eq fr2,fr5             fcmp/eq fr0,fr5             fcmp/eq fr1,fr5
        fmov    fr5,fr1             fmov    fr5,fr1             bt/s    .L17
        fmov    fr5,fr0   <- x      fsrra   fr1                 fmov    fr5,fr0   <- x
        fsrra   fr1                 bt      .L7                 fmov    fr5,fr1
        bt      .L4                 fmov    fr5,fr0             fsrra   fr1
        fmul    fr1,fr0             rts                         fmul    fr1,fr0
    .L4: rts                        fmul    fr1,fr0         .L17: rts
         nop                    .L7: rts                         nop
                                     nop

The test is there in all three; nothing is folded away. What differs is what the zero path returns. In `plain` and in SH4ZAM's own routine, fr0 already holds x when the branch is taken: the routine returns its argument. In `barrier` fr0 holds a +0 that was really loaded. The pass that does it, from the dumps (the PHI that selects the returned value, before and after `uncprop1`):

    plain:    # _1 = PHI <0.0(5), r_5(3)>      ->   # _1 = PHI <x_3(D)(5), r_5(3)>
    barrier:  # _1 = PHI <r_5(3), value_12(4)>  ->   unchanged: value_12 is an asm output, there is no constant to replace

`uncprop` ("un-constant-propagation") replaces a constant by a variable known to equal it on that edge, to save the constant's load. For floats it may do so only when the sign of a zero does not matter: `-fno-signed-zeros` alone is enough to trigger it, and adding `-fsigned-zeros` to the fast flags gives the constant back. The bits, run in the interpreter (callee in its own object file):

    argument            +0          -0          denormal 0x00000001   4.0
    plain, fast         00000000    80000000    00000001              40000000
    shz_sqrtf, fast     00000000    80000000    00000001              40000000
    barrier, fast       00000000    00000000    00000000              40000000
    all three, strict   00000000    00000000    00000000              40000000

Two notes. With FPSCR.DN = 1 the SH-4 compares a denormal as equal to zero, so the zero path is taken for a denormal and "return x" hands that denormal back. And the swap depends on the caller: inlined into a function that wants the result's bits in an integer register, the same source gave `mov #0,r0` on the zero path without any barrier. So without the barrier the zero path is +0 or the argument, depending on where the routine is inlined; with it, always +0.

I think this is what reached you as "the compiler folds the zero check unless there is an opaque barrier". The check stays. The constant behind it becomes the argument.

#### B.2.2 Regrouping a product (reassoc1)

The arccosine is `r = FSRRA(1 - t) * ((1 - t) * P(t))`, P a quartic, then `if (t >= 1) r = 0`. In a loop over an array, GIMPLE before `reassoc1` and after it:

    before, no barrier:              _95 = one_minus_79 * _94;   r_97 = x_85 * _95;        x_85: FSRRA's result, _94: P(t)
    after,  no barrier:              _7  = one_minus_79 * x_85;  r_97 = _7 * _94;          regrouped: (1 - t) * FSRRA first
    after,  shz_dce_opaquef:        _95 = _94 * one_minus_79;   __asm__("" : "=f" value_96 : "0" _95);   r_98 = value_96 * x_85;
    after,  __builtin_assoc_barrier: _95 = _94 * one_minus_79;   _96 = ((_95));            r_98 = _96 * x_85;

Without a barrier the multiply by FSRRA's result is issued first and waits for FSRRA with the polynomial queued behind it. `reassoc` orders the factors by its own rank of each value, so the result depends on the surroundings: inside this loop it moved FSRRA's result FIRST; in a standalone function with x in a register it leaves `inv * (a * p)` alone and turns `(inv * a) * p` into `(a * p) * inv`, FSRRA's result LAST. `-fno-associative-math` stops it in both. One consequence: without a barrier the last bit of asinf / acosf can differ from one call site to another. The 5.8e-6 bound does not care.

#### B.2.3 Sinking below a branch (sink1)

After the product comes the range test, and r is only needed when t < 1. `sink1` therefore moves the work to the side of the branch that uses it. Its own log for the three variants:

    no barrier:                 11 statements sunk   (both multiplies and the whole polynomial)
    __builtin_assoc_barrier:    12 statements sunk   (the same, and the barrier expression itself)
    shz_dce_opaquef:            1 statement sunk    (the last multiply only)

An asm statement is not an assignment and `sink1` does not move it, so the barrier, and with it the product and the polynomial that feed it, stay in front of the test. This is what 15.2 does, not a promise: the manual says "the compiler can move even volatile asm instructions relative to other code, including across jump instructions". Two results, by the model (cycles E an element, loop over an array):

                                  acosf in range   acosf past 1   asinf in range   asinf past 1
    shz_dce_opaquef                  28.45           29.00          28.52           30.00
    no barrier                        31.45           21.00          34.52           21.00
    __builtin_assoc_barrier           32.45           21.00          33.52           21.00

Past 1 the barrier makes the clamp path pay for the polynomial: your "x in +-pi" rows, 46 / 202 -> 65 / 210 and 54 / 256 -> 68 / 270. In range the model promised 3 to 6 cycles for the barrier in a loop; your console says 64 against 65 on single calls, a tie. So the model is wrong there, or the gain is under your harness's resolution; either way the barrier buys nothing you can measure in asinf / acosf and costs the out-of-range path. Take them WITHOUT it: that is your "theirs" column.

#### B.2.4 Smaller things that also stop at it

    return shz_dce_opaquef(2.0f) * 3.0f;       both constants loaded, fmul at run time (not folded to 6.0f)
    return c + a * b;                           fmov fr5,fr0 / fmac fr0,fr4,fr7
    return c + shz_dce_opaquef(a * b);         fmov fr5,fr0 / fmul fr4,fr0 / fadd fr7,fr0      (no contraction through it)

### B.3 What it costs

No instruction of its own. But:

- a constant passed through it has to exist: `fldi0`, and the zero path becomes a block of its own (`shz_dce_sqrtf` ends in `.L19: fmov fr2,fr0 / rts / nop` where the plain form shares one `rts`). Four roots in a row carry four such blocks, which is at least consistent with your "sqrtf x4" row: 59 / 138 without the barrier, 66 / 156 with it.
- it can take an instruction out of a delay slot:

      return a * b;                       fmov fr5,fr0 / rts / fmul fr4,fr0          3 instructions
      return shz_dce_opaquef(a * b);     fmov fr5,fr0 / fmul fr4,fr0 / rts / nop    4

- B.2.3: whatever feeds it is computed before a following branch, wanted or not.

### B.4 What it does not do

- It is not `volatile`. The manual: "GCC's optimizers sometimes discard asm statements if they determine there is no need for the output variables." If its result is unused it disappears together with the computation of its operand (`(void)shz_dce_opaquef(a * a);` compiles to `rts / nop`), and an operand that does not change in a loop is still computed once, before the loop. Two barriers on the same value were not merged, which costs nothing either way.
- It protects nothing inside its parentheses: `shz_dce_opaquef(a * b * c)` may still be regrouped inside.
- It does not order instructions inside a block: the scheduler still places FSRRA where it likes. Only a real asm block fixes an order.
- It exists on the SH-4 only (`f` is SH's letter); on any other target the function returns its argument and nothing is hidden.

### B.5 `__builtin_assoc_barrier(x)` and the other candidates

The builtin does B.2.2 (its documentation adds: it also prevents contraction across it) and neither B.2.1 nor B.2.3. Around a constant the front end drops it, so `x == 0.0f ? __builtin_assoc_barrier(0.0f) : r` compiles to the same instructions as the plain form and returns the argument. In the arccosine it keeps the grouping, and `sink1` moves it below the test with everything else.

For the zero path the others, same test (instruction lines of the small function above, fast flags; the plain form has 9 and returns the argument): a `volatile float` temporary 14 (a store, a reload, two stack adjusts), +0 kept; `always_inline` with `__attribute__((optimize("no-fast-math")))` 9, the attribute is lost on inlining and the argument comes back; `#pragma GCC optimize("no-fast-math")` in front of a function that is not inlined 9, +0 kept, at the price of that function being built strict and called; `#pragma STDC FP_CONTRACT` is ignored with a warning.

### B.6 On +-0 and denormals

The barrier tells the compiler nothing about zeros; it only withholds something. The flag is what speaks: `-fno-signed-zeros` (part of `-ffast-math`) says the sign of a zero does not matter, and that is what makes x an acceptable stand-in for the constant 0.0 on the path where x == 0. On the SH-4 a zero is a zero, not a denormal. What is special is FPSCR.DN = 1, as KallistiOS runs: every FPU instruction, compares included, reads a denormal operand as zero (in the SH-4 manual's own pseudo-code, `data_type_of()` returns a zero for a denormal when DN = 1, and FCMP compares on that). Hence `x == 0.0f` is true for a denormal x, and a routine that then returns x returns bits that are not zero but that the FPU reads as zero. Harmless in arithmetic. Visible if the result is stored and compared as bytes, hashed, or sent to something that runs with DN = 0.

### B.7 Where SH4ZAM-DCE uses it now

- `shz_dce_sqrt.h`, the zero constants of the guarded roots and lengths, a switch a group (`SHZ_DCE_OPAQUE_SQRT`, `_INV_SQRT`, `_MAGNITUDE`, `_DISTANCE`, `_NORMALIZE2 / 3 / 4`): on for `inv_sqrtf` and the vec2 normalisation, where the console preferred it, off elsewhere. Both states are tested against SH4ZAM 0.10's routines bit for bit.
- `shz_dce_trig.h`, around `(1 - t) * P(t)`: `SHZ_DCE_OPAQUE_ASIN_ACOS`, off. The file for upstream has none.
- `shz_dce_project.h`, plain C form: not load-bearing. The fused step next to it is itself an asm statement; with and without the barrier the compiler emits the same arithmetic (176 instructions either way).
