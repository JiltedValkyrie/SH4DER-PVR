# Hardware probe disc

A small test disc: one PowerVR2 hardware mode a screen, each with a label, what you should see, and a verdict the program makes by reading its own finished frame back. It exists so that an effect is only built on a mode after that mode has been seen working on a real Dreamcast.

```sh
make probe SH4ZAM=/path/to/sh4zam PROBE_CDI=build/probe/sh4der-probe-b3.cdi    # KallistiOS environment sourced; needs mkdcdisc
```

| Screen | Mode | You should see | The verdict compares |
| --- | --- | --- | --- |
| 0 BASE | plain opaque quads | a red, a green and a blue bar | each bar reads as its own colour (this proves the frame read; if it fails, every later verdict is N/A) |
| 1 CHEAP SHADOW | opaque modifier volume, cheap shadow register at 0.5 | a blue grid square with a darker block sliding over it, hard edges | inside is 25 to 75 % as bright as outside |
| 2A TWO-VOLUME, OPAQUE | two-volume polygon, opaque list; closed volume; cheap shadow register off | a blue grid square; the sliding block shows an orange grid | inside is orange, outside is blue |
| 2B | the same, drawn exactly as the KallistiOS example does: one open quad as the volume, in front of the surface; register as `pvr_init` leaves it | as 2A | as 2A |
| 2C | closed volume; register as `pvr_init` leaves it | as 2A | as 2A |
| 2D | as 2C with the polygon's shadow bit cleared | as 2A, or no block at all (either is a finding) | as 2A |
| 2E | as 2C with no texture: two vertex colours, 32-byte vertices | a flat blue square with a flat orange block | as 2A |
| 3 TWO-VOLUME, TRANSLUCENT | two-volume polygon, translucent list and translucent modifier list | a grey panel; only the sliding block shows the orange grid, nothing else of the square | inside is orange, outside is the grey panel |
| 4 FOG MODE 2 | fog table mode 2 beside mode 1 | top row: white, pale, cyan patches. Bottom row: no patch, a faint one, a cyan one | bottom row: near patch absent, far patch cyan ("LIKE MODE 1" if the near patch is white) |
| 5 BUMP | bump texture format, as a polygon (left) and a sprite (right) | two squares of pyramids, lit from a light that turns round them | east faces brighter than west faces, on both |
| 6 CLAMP | colour clamp registers at 0.25 and 0.75 | two black-to-white ramps; the lower one is dark grey to light grey | the lower ramp is brighter at the dark end and darker at the bright end |

Every moving part holds still for one second, the read happens in that second, then it moves. One serial line a screen, once, and one closing line when all seven have been read. Start-up paints the frame red, orange, yellow, green, magenta in turn (one line each on the serial port); white later means the PVR never finished a frame, and the serial line names the screen.

Pad: A or D-pad right next, D-pad left previous, Y automatic cycling on / off (on at start, six seconds a screen).

**Two-volume polygons (opaque and translucent) are PARKED.** They failed on a real console in build 2; the cause was not found; the header words build 3 prints were decoded and are what was intended. Nothing in the library uses the mode. Build 3's five variant screens (2A to 2E) are there for whoever picks it up.

Results so far, build 2 on a real console: screens 0, 1, 4, 5 and 6 pass (cheap shadow through a modifier volume, fog table mode 2, the bump format, the colour clamp registers). Screen 2 (two-volume, opaque) and screen 3 (two-volume, translucent) fail. Build 3 splits screen 2 into five variants to find out why, and prints every header's eight words on the serial port at start-up.

Cost: 88 KB of video memory in textures, about 70 KB of main RAM, no sound. Safety: every header goes through `sh4der_poly_hdr_ok` and every volume triangle through `sh4der_tri_area_ok`; a volume with a rejected triangle is not sent (never an open volume), and the closing serial line counts rejections.
