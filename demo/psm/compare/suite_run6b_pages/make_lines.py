"""Suite s6b: the values typed from the photos, put through each program's own page formats (the say() calls named
below), so tools/page_crc.py's line cut and CRC can check them. Writes <entry>.lines here (one logical line a say()).
Scholar, 2026-11-25. Photos: logs/benchmarks/suite_run6b (the owner's drive)."""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
REV = 'HOLLY REV 0011 SB 0B'


def out(name, lines):
    with open(os.path.join(HERE, name + '.lines'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


# hero1, hero/hero_spike.c:738-768. Photos 175011514 (p1, CRC 9a95f13e), 175015334 (p2, CRC 9eb3d683)
L = ["hero1 h1c: a skinned hero, each pass's cost. This run's numbers.",
     "%s. %u s a stage, the first 0.5 s not counted; ms unless said" % (REV, 5),
     "hero: built-in (original), 1465 vertices, 1660 triangles, 515 strips, 33 bones, clips 60+24 frames"]
st = [("all, IDLE", 18.79, 7.40, 10.56, 8603, 291, 238),
      ("all, RUN+AIM", 18.41, 7.31, 10.35, 8435, 285, 243),
      ("opaque only", 16.56, 3.81, 4.72, 3105, 112, 271),
      ("opaque+detail", 16.55, 5.27, 5.69, 5857, 212, 271),
      ("opaque+visor", 16.55, 3.93, 4.74, 3267, 119, 271),
      ("opaque+shadow", 16.55, 5.79, 9.52, 5698, 184, 271),
      ("no hero (set)", 16.56, 0.37, 3.97, 488, 13, 270),
      ("all, room", 23.70, 12.11, 11.37, 16689, 412, 189),
      ("all, room+hens", 23.56, 12.74, 10.60, 16163, 380, 190),
      ("all, 2x distance", 17.76, 7.35, 9.82, 8511, 293, 252)]
for k, s in enumerate(st):
    L.append("%u %-16s fr %5.2f SH4 %5.2f PVR %5.2f TA %4uv %3uK n%u" % ((k + 1,) + s))
L += ["stage 2 SH-4 parts: anim 0.07 palette 0.03 skin 0.81 light 0.40",
      "  sends: opaque 2.14 detail 1.37 visor 0.08 shadow 2.08 set 0.34",
      "  PVR: shadow target 3.46, screen 6.89; TA map 72K screen 214K",
      "DERIVED, each pass over stage 3 (opaque only): SH-4 / PVR ms, TA vertices"]
for p, a, b, c in [("detail", 1.46, 0.96, 2752), ("visor", 0.12, 0.02, 162), ("shadow", 1.98, 4.80, 2593)]:
    L.append("  %-7s SH-4 %+.2f PVR %+.2f TA %+d v" % (p, a, b, c))
L += ["  hero, opaque (stage 3 - 7): SH-4 +3.44 PVR +0.76 TA +2617 v",
      "  room over clay (8 - 2): SH-4 +4.79 PVR +1.02, 2781 room tri drawn",
      "  4 hens (9 - 8): SH-4 +0.63 PVR -0.77 TA -526 v",
      "  2x distance (10 - 2): frame -0.65 PVR -0.53",
      "culling off on the hero (closed tubes): a back-face cull is not tried here"]
out('hero1', L)

# hens-eyes, ao/hen_spike.c:501-519. Photos 175141056 (p1, CRC 0a760cfb), 175143700 (p2, CRC 4f62a4b9)
L = ["hens a: rays against a sweep, the same hens and room.",
     "%s. Cycles a step of 1/60 s, all hens (PRFC1)." % REV,
     "pre: the eyes or the sweep. post+sep: the push, hens apart.",
     "model: the modules' cost tables on the counts, ESTIMATE.",
     "world 9 objects 1000 triangles; grid 16x16, 788 items",
     "9 rays + 2 down, tick 100 ms; sweeps look as far as the eyes"]
T = [(4, "EYES", 11233, 52508, 2914, "/100 steps: tick 66 ray 735 box 4214 tri 5289 pair 234", 6649, 299, 301, "16.5", 4972),
     (4, "SWEEP GRID step", 49810, 87070, 3225, "/100 steps: cast 400 tri 5481 rej 4785 cell 4305 pair 1200", 15716, 299, 301, "16.5", 4832),
     (4, "SWEEP GRID tick", 5392, 37283, 2788, "/100 steps: cast 66 tri 343 rej 452 cell 717 pair 200", 1429, 299, 301, "16.5", 4898),
     (4, "SWEEP NAIVE", 324094, 359975, 3821, "/100 steps: cast 400 tri 5481 rej 394518 cell 0 pair 1200", 93016, 299, 301, "16.5", 4874),
     (16, "EYES", 44130, 118739, 11171, "/100 steps: tick 267 ray 2943 box 16856 tri 17872 pair 9484", 38663, 299, 301, "16.5", 5548),
     (16, "SWEEP GRID step", 221541, 296832, 13029, "/100 steps: cast 1600 tri 25795 rej 22612 cell 18488 pair 24000", 98735, 299, 301, "16.5", 5101),
     (16, "SWEEP GRID tick", 36328, 112992, 11170, "/100 steps: cast 267 tri 3306 rej 4117 cell 2973 pair 4013", 14877, 299, 301, "16.5", 5153),
     (16, "SWEEP NAIVE", 1321198, 1401483, 16120, "/100 steps: cast 1600 tri 25795 rej 1574204 cell 0 pair 24000", 406280, 299, 219, "22.8", 5096),
     (32, "EYES", 98683, 202899, 27334, "/100 steps: tick 533 ray 5869 box 33614 tri 37186 pair 39448", 113460, 298, 251, "19.8", 6313),
     (32, "SWEEP GRID step", 444943, 517117, 31710, "/100 steps: cast 3200 tri 46217 rej 49486 cell 35430 pair 99200", 256857, 299, 241, "20.7", 5433)]
for n, mode, pre, worst, post, cnt, model, steps, frames, ms, pvr in T:
    L.append("%2u %s: pre %u worst %u; post+sep %u" % (n, mode, pre, worst, post))
    L.append("  %s" % cnt)
    L.append("  model %u; %u steps %u frames %s ms, PVR %u us" % (model, steps, frames, ms, pvr))
out('hens_eyes', L)

# The town autorun pages, demo/psm/psm_dc.c auto_pages() (ao1c: Junior's ao1c_src, PSM_AO_DEMO; hud1c-3: hud1c_src, no
# revision line and no AO lines).
EV = "  events ISP 0 strip 0 PARAM %u OPB %u in-err 0 in-ovf 0%s"


def town(name, head, segs):
    L = list(head)
    for nm, n, tag, fr, sh, parts, pvr, tav, mp, used, est, kos, shed, param, opb in segs:
        L.append("%s (%u fr)%s: frame %s  SH-4 %s" % (nm, n, tag, fr, sh))
        L.append("  parts %s  PVR %s  TAv %s" % (parts, pvr, tav))
        if mp: L.append("  map TAv %s  map KB %s" % mp)
        L.append("  used KB %s  est KB %s  KOS max %uK  SHED<=%u  text 92v" % (used, est, kos, shed))
        L.append(EV % (param, opb, "  OVERFLOW" if param + opb else ""))
    L.append("END. OVERFLOW: the TA ran out of vertex buffer or bins.")
    out(name, L)


AOH = ["TOWN AUTORUN ao1: 1350 frames counted, sim step 1/30 s",
       "vertex buffer 1024 KB x2; one console (%s); per segment min/mean/max" % REV,
       "ms unless said. SH-4 = setup+map+casters+compare. PVR = last done render",
       "AO1 baked: walls+ground, 0 PVR. AO2 PVR map: ground only, K 4, union",
       "map = route A's map pass alone (in TAv); screen = TAv - map"]
# ao1c, first run. Photos 175338338 (p1, CRC bd80c910), 175340403 (p2, 837cfa1f), 175342282 (p3, 5667072b)
town('ao1c_run1', AOH, [
    ("OFF still", 150, "", "51.2/53.3/87.0", "13.2/13.8/27.2", "2.6 7.6 3.7 0.0", "3.3/3.9/14.6", "27746/27802/27903", ("3215/3220/3242", "63/63/63"), "63/604/976", "699/701", 975, 0, 0, 150),
    ("OFF turn", 300, "", "51.4/68.2/100.6", "13.3/29.9/43.2", "19.1 7.1 3.7 0.0", "3.3/4.0/15.9", "24084/26068/28021", ("2702/2923/3218", "53/57/63"), "53/665/996", "674/710", 996, 0, 0, 300),
    ("AO1 still", 150, "", "51.9/54.4/87.3", "13.2/14.2/31.2", "2.7 7.8 3.7 0.0", "3.3/3.8/14.1", "27790/27850/27900", ("3215/3215/3215", "63/63/63"), "63/610/976", "700/701", 996, 0, 0, 150),
    ("AO1 turn", 300, "", "54.8/69.1/110.9", "13.3/29.9/42.4", "19.1 7.1 3.6 0.0", "3.3/4.1/16.2", "24233/25989/27981", ("2609/2903/3227", "51/57/63"), "51/614/994", "673/709", 996, 0, 0, 300),
    ("AO2 still", 150, "", "60.7/63.3/94.2", "22.8/23.6/41.0", "2.7 17.3 3.7 0.0", "4.7/5.3/14.1", "39333/39366/39411", ("14750/14775/14816", "288/289/289"), "288/741/975", "699/699", 996, 0, 0, 150),
    ("AO2 turn", 300, "", "60.9/77.8/110.2", "23.0/39.3/52.8", "19.1 16.5 3.7 0.0", "4.3/5.2/15.9", "35073/37023/39540", ("12635/13892/15209", "247/271/297"), "247/740/996", "674/709", 996, 0, 0, 300)])
# ao1c, second run. Photos 180547346 (p1, 5db8773e), 180550348 (p2, 160d537d), 180551698 (p3, 7a55b89e)
town('ao1c_run2', AOH, [
    ("OFF still", 150, "", "51.2/53.3/87.1", "13.2/13.8/27.3", "2.5 7.6 3.7 0.0", "3.3/3.9/14.6", "27746/27802/27903", ("3215/3220/3242", "63/63/63"), "63/604/976", "699/701", 975, 0, 0, 150),
    ("OFF turn", 300, "", "51.3/68.1/100.3", "13.2/29.9/43.0", "19.1 7.1 3.7 0.0", "3.3/4.0/15.6", "24084/26068/28021", ("2702/2923/3218", "53/57/63"), "53/640/996", "674/710", 996, 0, 0, 300),
    ("AO1 still", 150, "", "51.8/54.3/97.8", "13.2/14.1/40.4", "2.7 7.7 3.7 0.0", "3.3/3.8/14.1", "27790/27850/27900", ("3215/3215/3215", "63/63/63"), "63/610/976", "700/701", 996, 0, 0, 150),
    ("AO1 turn", 300, "", "54.1/69.1/100.7", "13.3/29.9/43.4", "19.1 7.2 3.7 0.0", "3.3/4.1/17.0", "24233/25989/27981", ("2609/2903/3227", "51/57/63"), "51/640/994", "673/709", 996, 0, 0, 300),
    ("AO2 still", 150, "", "60.7/63.3/92.7", "22.8/23.6/40.9", "2.7 17.3 3.7 0.0", "4.7/5.3/14.2", "39333/39366/39411", ("14750/14775/14816", "288/289/289"), "288/728/975", "699/699", 996, 0, 0, 150),
    ("AO2 turn", 300, "", "60.8/77.7/110.1", "22.8/39.3/52.7", "19.1 16.5 3.7 0.0", "4.3/5.2/16.5", "35073/37023/39540", ("12635/13892/15209", "247/271/297"), "247/717/996", "674/709", 996, 0, 0, 300)])
# hud1c-3. Photos 180348161 (p1, de11a326), 180351495 (p2, 35a651c3)
town('hud1c3', ["TOWN AUTORUN hud1c-3: 1350 frames counted, sim step 1/30 s",
                "vertex buffer 1024 KB x2; one console; per segment min/mean/max",
                "ms unless said. SH-4 = setup+map+casters+compare. PVR = last done render"], [
    ("A normal", 150, "", "59.0/61.9/93.3", "12.7/13.5/26.4", "2.7 6.8 4.0 0.0", "3.3/3.9/12.1", "30541/30613/30624", None, "65/909/1024", "754/754", 1024, 0, 150, 150),
    ("A 1ST", 150, " 1ST", "59.0/61.9/96.9", "12.7/13.5/30.0", "2.7 6.8 4.0 0.0", "3.3/4.0/16.6", "30596/30622/30624", None, "66/768/1024", "754/754", 1024, 0, 150, 150),
    ("A S3", 150, " S3", "51.3/53.6/84.3", "12.7/13.4/27.3", "2.7 6.7 4.0 0.0", "3.3/3.6/9.0", "22124/22135/22136", None, "600/601/601", "754/754", 1024, 3, 0, 150),
    ("A norm trn", 300, "", "59.3/78.3/111.2", "12.8/31.5/43.8", "21.1 6.4 4.0 0.0", "3.3/4.1/14.0", "24908/29012/30640", None, "50/749/1024", "723/754", 1024, 0, 52, 300),
    ("A 1ST trn", 300, " 1ST", "67.1/78.6/111.1", "28.3/31.6/44.1", "21.1 6.5 4.0 0.0", "3.3/4.1/14.3", "25036/29083/30645", None, "54/755/1024", "724/754", 1024, 0, 47, 300),
    ("A S3 trn", 300, " S3", "59.5/69.9/101.1", "28.2/31.4/45.6", "21.1 6.3 4.0 0.0", "3.3/3.7/9.0", "16477/20530/22082", None, "453/538/600", "723/753", 1024, 3, 0, 300)])
