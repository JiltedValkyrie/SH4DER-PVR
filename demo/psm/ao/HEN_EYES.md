# Hen eyes: rays in place of a collision sweep

A test of one idea: an agent that walks on a floor (a hen here; an enemy in a shooter is the real question) looks with a few rays on a timer, in place of sweeping its body against the world every frame. This page says what was built, what it costs by our ledger, and how it compares with a conventional sweep on the same scene.

**Status: host only.** Nothing here has run on a Dreamcast. Every count below is exact and repeatable. Every cycle and microsecond figure is a model: **ESTIMATE**. One constant in the model is measured (a ray against a triangle, 62.49 cycles, SH4-066); the rest are guesses that the console entry must replace.

## The short answer

For 8 agents at 30 frames a second, in the scripted room:

| Method | us a frame, mean | us, worst frame | Share of a 33.3 ms frame, mean |
|---|---|---|---|
| Sweep every frame against every triangle (no broad phase) | 378 | 462 | 1.13 % |
| Sweep every frame, grid broad phase, looking as far as the eyes | 139 | 246 | 0.42 % |
| **Eyes: 9 rays, tick 0.1 s** | **105** | **260** | **0.31 %** |
| Eyes: 5 rays, tick 0.1 s | 51 | 97 | 0.15 % |
| Sweep every frame, grid, looking only speed x dt ahead | 56 | 73 | 0.17 % |
| Sweep on the same 0.1 s tick, grid | 31 | 158 | 0.09 % |

Read it this way:

- **Against a sweep with no broad phase, the eyes are 3.6 times cheaper** (9 rays), 7 times with 5 rays. If the port's sweep tests every triangle every frame, this is the gain, and it grows with the triangle count: that sweep's cost follows the scene's triangles, the eyes' cost does not.
- **Against a sweep with a grid, the eyes are about level** (105 against 139 us). The rays are not the saving.
- **The saving is the tick.** The same sweep run every 0.1 s costs 31 us: a third of the eyes. If the port only needs "do not walk into things", put its sweep on a timer with a per-frame push and stop there.
- **What the eyes add that a sweep cannot:** distance in every direction of the fan (so an agent turns toward the open side), the floor's edge, and line of sight to the viewer through the same triangles.
- All of these are small. Even 32 agents with 9 rays are 0.7 ms a frame (2.1 %), 1.2 ms on the worst frame.

## What was built

| File | What |
|---|---|
| `hen_eyes.h` | The module. Header only. The world (objects, boxes, triangles), the ray kernels, the tick, the steering, the per-frame push, the debug segments, the counters and the cost model. |
| `hen_sweep.h` | The baseline: a swept body against the room (every triangle, object boxes, or a 2D grid) and against the other agents. It writes the same decision into the same fields. |
| `test/hen_eyes_test.c` | The host test (28,157 checks) and the tables (`--report`). |
| `HEN_EYES_REPORT.md` | The full tables as the test printed them. |

**How an agent sees.**

- **Fan:** 5, 9 or 13 level rays at eye height (0.20 m) over 120 degrees. Each returns the distance to the nearest triangle of the room, or to another agent (a circle).
- **Down:** two rays straight down, ahead and to each side. No floor under one is an edge: the agent turns away from it.
- **Sight:** one ray to the viewer, only when the viewer is inside the fan and within 6 m. Furniture in the way hides the viewer. The eyes can then freeze the agent, turn it to look, or ask its walk to run away.
- **Tick in seconds:** 0.05, 0.1, 0.2 or 0.4 s, each agent with its own random phase, so they are spread over the frames. A frame longer than the tick casts once.
- **Between ticks:** no ray. The agent turns toward the last answer at a fixed rate (4 rad/s) and keeps the last speed factor.
- **Every frame, the push:** the body's circle is moved out of each object's footprint rectangle and kept on the floor. This is what makes "never through furniture" a guarantee, whatever the rays missed.
- **Reach:** at least speed x tick x 2 plus the body (1.2 m at 0.1 s for a 2 m/s run; 1.8 m at 0.4 s).
- **The agent's own walk is untouched.** The eyes change the bearing, scale the frame's step by 0 to 1, and push. The hens keep their peck, step and run, and those that keep to a small area still do.

**What keeps it cheap.**

- A level ray can only hit a triangle that spans the eye's height. The world sorts those first: 104 of the scripted room's 372 triangles (5 KB, inside the 16 KB cache).
- A ray tests an object's box (two slabs) before its triangles, and skips a box it enters beyond its nearest hit.
- The nearest hit is kept by a multiply, with one reciprocal only when a hit is nearer. No divide in a loop.
- Other agents are looked up once a tick; a ray tests only those within reach.

## One tick of one agent (9 rays, 0.1 s)

| | Count (mean) | Cycles (ESTIMATE) |
|---|---|---|
| Rays | 11.3 (9 fan, 2 down, 0.3 sight) | 74 each, set-up |
| Box tests | 93.0 | 30 each |
| Triangle tests | 24.4 | 62.49 each (the one MEASURED figure) |
| Other agents: distance looks / ray tests | 7.0 / 9.3 | 20 / 137.87 each |
| Steering | 1 | 150 |
| **A tick** | | **6,721 cycles = 33.6 us** |

- Cast between other work in place of in one batch, a ray cost up to 164 cycles more on the console (SH4-062). That would make a tick 8,575 cycles, 42.9 us.
- **Ticks in 1 ms: 30.** Agents in 1 ms a frame at 30 fps: 88 on the mean (a third of them tick each frame), 30 if every one ticked on the same frame.
- The worst frame seen with 8 agents had 4 ticking together.

## Agents and frame length (eyes, 9 rays, 0.1 s tick, against the grid sweep every frame)

| Agents | Eyes: us mean / worst | Share of 33.3 ms | Grid sweep: us mean / worst | Share | Sweep, no broad phase: us mean |
|---|---|---|---|---|---|
| 4 | 38 / 111 | 0.11 % | 55 / 186 | 0.16 % | 191 |
| 8 | 105 / 260 | 0.31 % | 139 / 246 | 0.42 % | 378 |
| 16 | 250 / 466 | 0.75 % | 281 / 443 | 0.84 % | 861 |
| 32 | 697 / 1,212 | 2.09 % | 1,007 / 1,218 | 3.02 % | 2,121 |

- Both grow faster than the agent count, because each agent also tests the others. Past about 16 agents that part is the larger share.
- **Frame length (8 agents).** The eyes' cost a second is fixed by the tick, so a longer frame carries more of it: 62 us in a 23 ms frame, 105 us in 33 ms, 200 us in 75 ms. The share stays near 0.3 %. A sweep every frame costs the same each frame (about 100 to 140 us) whatever the frame's length.
- On top of either: the shared push and the agents' separation, 23 us a frame for 8 agents, 130 to 146 us for 32 (the separation is every pair).

## Outcome on the same scripted run (60 s, 8 agents)

| Method | Frames with a body inside furniture | Deepest overlap left, m | Moved by the push in all, m | Walked in all, m |
|---|---|---|---|---|
| Nothing (the walk alone) | 768 | 0.15 | — | 119.7 |
| Push only | 0 | 0 | 25.6 | 104.2 |
| Eyes, 5 rays | 0 | 0 | 0.4 | 104.5 |
| Eyes, 9 rays | 0 | 0 | 1.0 | 101.8 |
| Eyes, 13 rays | 0 | 0 | 7.2 | 103.6 |
| Sweep, grid, every frame, same reach | 0 | 0 | 0.0 | 102.2 |
| Sweep, grid, every frame, speed x dt | 0 | 0 | 7.2 | 76.6 |
| Sweep, grid, on the tick | 0 | 0 | 0.0 | 102.9 |

- **No method lets a body through furniture or another hen:** the push and the separation see to that in all of them.
- "Moved by the push" says how much the safety net had to do. Steering by eyes cuts it from 25.6 m to about 1 m. The sweep with the same reach cuts it to 0.
- More rays did not steer better here (13 rays: 7.2 m). The steering rule's weights are a first guess, tuned by nobody. OPEN.
- **The paths are not the same paths.** The walk has random turns, so once two rules turn a hen differently they part: 0.30 m apart after 5 s, 1.70 m on the mean over the run. The distance walked is the same (101.8 against 102.2 m).

## Where each one wins

**The sweep wins:**

- **Thin obstacles.** A 4 cm post 10 cm off the path: the sweep reacts 0.36 to 0.64 m away. Five rays react at 0.03 m (the post sits between two rays until the body is on it); nine at 0.27 m; thirteen at 0.52 m. Only the push saves the 5-ray agent.
- **Other agents.** A swept body knows its own width. With 32 agents the eyes left the push 17 m of work and the sweep none.
- **Fast movers with a long tick.** Running at 2 m/s into a wall on a 0.4 s tick, the eyes touched the wall and the push moved the body 0.30 m. On a 0.1 s tick, nothing touched.
- **Cost, when it has a grid and runs on the same tick:** 31 us against 105.

**The eyes win:**

- **Against a sweep with no broad phase:** 105 us against 378, and the gap widens with the scene's triangle count.
- **Warning distance for the price.** A plain sweep of speed x dt reacts at 0.05 to 0.11 m, slides along things, and walked 76.6 m where the others walked 102. To see as far as the eyes it must sweep 1 m ahead every frame, which is the 139 us row.
- **Sight.** The same world and kernel answer "can it see the viewer" for 0.3 rays a tick. A sweep gives nothing for that.
- **The edge of a floor**, from the two rays down.
- **A steady cost a second**, whatever the frame rate.

## What is not known

- **Every cycle figure but one.** The triangle test's 62.49 cycles was measured for an any-hit test with the triangles in the cache. The nearest-hit form here, the box test, the capsule test and the push have no console row. The sweep's capsule-against-triangle figure (220 cycles) is a guess; halve or double it and the grid sweep moves between about 100 and 220 us for 8 agents. The ranking against the eyes can flip.
- **Out of the cache.** The fan's triangles fit (5 KB). The sight ray reads every triangle of an object it enters (18 to 25 KB in all): out of the cache, not measured.
- **The real room.** The tables are from the scripted room (372 triangles). With the placeholder `room_data.h` (520 triangles) the test passes and the eyes cost more (146 us for 8 agents, 9 rays), because more triangles span the eye's height. The modelled room has not been run through it.
- **The steering's weights**, and whether 13 rays can be made to steer better than 9.

## Debug layer (for the console entry)

`hen_eyes_debug()` returns the last tick's rays as world segments with a colour each (at most 16 an agent). Draw each as a thin quad that faces the camera (`sh4der_spark_quad`), untextured, additive, about 1.5 pixels wide, checked with `sh4der_tri_area_ok`. Toggle with one button.

| Colour | Meaning |
|---|---|
| green | a fan ray that met nothing within reach |
| red | it met the room |
| yellow | it met another agent |
| blue / magenta | a ray down with floor / with none (an edge) |
| white / grey | the line to the viewer: clear / blocked |

8 agents with 9 rays are at most 96 quads, 192 triangles.

## Use

```c
#include "room_data.h"          /* optional: gives hen_world_from_room() */
#include "hen_eyes.h"

static hen_tri_t tris[ROOM_CT];
static hen_world_t world;
static hen_eyes_cfg_t cfg;
static hen_eyes_t eyes[HENS];
static hen_stats_t stats;

/* once */
hen_world_from_room(&world, tris, ROOM_CT, 0.20f, 0.35f);       /* eye height, body top */
hen_eyes_cfg_default(&cfg);                                     /* 9 rays, 0.1 s; set cfg.rays, cfg.tick, cfg.react */
for (i = 0; i < HENS; i++) hen_eyes_init(&eyes[i], &cfg, random_0_to_1());

/* every frame, each hen i; pos[][2] holds every hen's (x, z) */
slow = hen_eyes_pre(&world, &eyes[i], &cfg, dt, x, z, &bearing, top_speed, pos, HENS, i, viewer_xyz, &stats);
x0 = x; z0 = z;
/* ... the hen's own walk moves x, z; eyes[i].flee asks it to run ... */
hen_eyes_post(&world, &cfg, x0, z0, &x, &z, slow, &stats);
/* once a frame, after all hens */
hen_agents_separate(pos, HENS, cfg.radius, &stats);
```

- Bearings are radians from +z, clockwise seen from above: forward is (sin b, 0, cos b).
- The sweep takes the place of `hen_eyes_pre` only: `hen_sweep_pre(&world, &grid, HEN_SWEEP_GRID, every_frame, look, &eyes[i], &cfg, dt, x, z, &bearing, top_speed, pos, HENS, i, &sweep_stats)`. Build the grid once with `hen_grid_build`.
- `hen_stats_cycles()` and `hen_sweep_cycles()` turn the counters into the model's cycles. On the console, time the two `pre` calls with the cycle counter and print both.

## Test

```
gcc -O2 -std=gnu11 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Idemo/psm/ao \
    demo/psm/ao/test/hen_eyes_test.c -lm -o build/hen_eyes/hen_eyes_test
build/hen_eyes/hen_eyes_test --report
```

- Plain C: 28,157 checks, 0 failed. SH4ZAM's host path (drop the define, add `-Ithird_party/sh4zam/include`): the same. With `-DHEN_TEST_ROOM -I<dir of room_mesh.h>` (the placeholder room): 28,139 checks, 0 failed.
- What it checks:
  - the kernel against its C reference and against a double-precision test (20,000 rays);
  - the fan's ray and the sight line against a test of every triangle (4,000 each);
  - the sofa hides a viewer, an open line does not, and a viewer behind the hen is not looked for;
  - the three sweeps give the same hit;
  - 24 scripted runs of the eyes (5, 9, 13 rays; four ticks; fixed frames and frames of 23 to 75 ms): no body inside furniture, two runs identical, ray counts equal ticks x rays, ticks equal time / tick, the turn never over its rate, the reach never under two ticks of travel;
  - the walk alone does cross furniture (so the scene tests something), and the eyes leave the push less to do than the push alone.
