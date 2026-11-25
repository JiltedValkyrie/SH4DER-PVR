# c4 loop and the SH-4's caches: cost model (ESTIMATE)

Senior's notes, 2026-11-25, overnight item 4. Nothing here was measured on a console. Every input is a MEASURED ledger row (named); every output is ESTIMATE. The question: where could the operand cache, the store queue, OC-RAM mode and block copy take work off c4's SH-4 loop (band cut, projection, send)?

## What the loop costs now (MEASURED, SH4-045, sun 150°/26°)

| Bands | Vertices | Band cut | Send (mask's two passes) | Light fit | Map's 68 vertices |
|---|---|---|---|---|---|
| 1 code | 2,464 | 2,483 us | 820 us | 54 us | 13-15 us |
| 2 codes | 1,288 | 1,332 us | 440 us | 53 us | — |
| 4 codes | 692 | 744 us | 251 us | 53 us | — |

Per output vertex this gives:

- Band cut: about 1.0 us, which is about 200 cycles at 200 MHz.
- Send: 333 ns at 1 code (820 / 2,464), 342 ns at 2 codes, 363 ns at 4 codes, against 142 ns a vertex by store queue (BUS-003).

## Where each tool fits

### 1. Band cut: about 200 cycles a vertex, compute bound

- Working set: 26 faces × 4 corners, their codes and the band polygons. That is a few KB and fits the 16 KB operand cache whole.
- A cache miss costs 27.7 cycles (SH4-047). Even one miss a vertex would explain only about 14 % of the 200 cycles.
- So caching does little here. The gain is in the arithmetic:
  - `c4_cross_edge` divides for each crossing; FSRRA or one reciprocal a face edge, hoisted out of the band loop, would replace it;
  - the bands of one face step by a constant code, so the crossings move by a constant along each edge: an add a band instead of a lerp.
- ESTIMATE: down to 60 to 100 cycles a vertex, i.e. 2,483 → 750 to 1,250 us at 1-code bands.
- This is the biggest single SH-4 cost in c4. It is a code change, not a cache change.

### 2. Projection: 21 cycles in the cache, 58 out (SH4-023)

- If the band corners are written to an array and projected in a second loop, that array must stay in the cache. At 1-code bands it is 2,464 × 16 B = 39 KB, more than the 16 KB cache.
- Projecting each band's corners right after the cut, while they are in registers or in a line just written, keeps the 21-cycle figure.
- ESTIMATE: up to (58 − 21) × 2,464 ≈ 91,000 cycles ≈ 0.46 ms saved, if the current loop projects out of cache. **OPEN:** whether c4's loop does today. The c4b `vpool` placing (one pool, offsets, bsrc and bscr apart: SH4-046) removes the conflict misses but not the capacity misses.
- Placing matters as much as size: `mat_transform` is 13.16 cycles on a clean placing and 38.67 with buffers 1 MB apart (SH4-020, SH4-021).

### 3. Send: 333 ns against 142 ns a vertex

- The gap is about 190 ns a vertex, or about 0.47 ms at 1-code bands.
- Writing each projected vertex straight into the store queue (KOS direct render, 32 B a vertex) as it is made removes the copy to a vertex buffer and its later send.
- ESTIMATE: send 820 → about 350 us at 1-code bands, 440 → about 185 us at 2 codes.
- The vertex is never stored in RAM, so it also takes the 39 KB array out of item 2's capacity problem.

### 4. OC-RAM mode: no

- OC-RAM is no faster than a cache hit (SH4-011: 1,401 us against 1,446 us).
- It halves the cache for everything else; a cached 8 KB took 59 % longer (SH4-012).
- The c4 working set already fits in the cache, so OC-RAM gives nothing and costs the rest of the frame.
- Keep it for the ray bench's question (P2.C/D), not for c4.

### 5. Block copy: only where a copy cannot be avoided

- The fastest copy, `shz_memcpy32` or the store-queue copy, runs at 130 to 150 MB/s and does not care about the placing (SH4-030, SH4-031). `memcpy` at a bad placing is 12.8 MB/s.
- c4 copies the MAP's depth codes into the mask's source (bref in c4b's PT route). At 128² × 2 B = 32 KB that is about 0.22 ms by `shz_memcpy32`, against 2.5 ms by `memcpy` on a bad placing.
- Better than any copy: c4b's PT route draws from a copy in bref only for the colour check. In the product it reads the map's own texture and needs no copy.
- ESTIMATE: 0 to 0.2 ms, depending on the route.

## Sum, 1-code bands (ESTIMATE)

| Step | Now (MEASURED) | With the changes (ESTIMATE) |
|---|---|---|
| Band cut | 2,483 us | 750-1,250 us (arithmetic, item 1) |
| Projection | inside the band cut and send (not apart) | −0 to 460 us (item 2) |
| Send | 820 us | about 350 us (item 3) |
| Total SH-4 | about 3.4 ms | about 1.1-1.6 ms |

## A host model that would check it

- Count the operations a vertex in `c4_band_poly` and `c4_cross_edge`: divides, multiplies, compares. Do it for the current code and for the incremental form.
- Check that the incremental form gives the same corners within 1e-4 codes over every face, band width and sun in `c4_model_test.c`.
- For item 2, a cache simulator (16 KB direct mapped, 32 B lines) run over the address trace of the two loop orders. Count misses × 27.7 cycles.
- Each number becomes MEASURED only on a console page that times the band cut, projection and send apart, at 1, 2 and 4 codes.
