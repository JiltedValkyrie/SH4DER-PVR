#!/usr/bin/env python3
"""SH4DER-PVR: the AO images (ao0: method 2's hardware questions; ao4: the split-screen demo), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/ao/build.py [--spike ao0|ao4] --tag <tag> [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--host-cc <cc>]
                                [--sh4zam <checkout>] [--town-inc <dir> ...] [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing
and touches no file outside --out (default build/psm_ao) and --cdi-dir (default demo/psm/out/cdi, the one place the
images are kept; git ignores it). Steps, each refusing to go on when it fails:

  1 host tests  test/ao_layer_test.c (the method's maths) and the image's own (ao0_model_test.c or ao4_scene_test.c), plain C;
                with --town-inc (the folders of the town's generated headers) ao_layer_test.c again with the town's sink
  2 elf         kos-cc <spike>_spike.c -> <out>/psm-<spike>-<tag>.elf, the library's Dreamcast flags, SH4ZAM's headers from
                --sh4zam (default third_party/sh4zam; the checkout is only read)
  3 image       mkdcdisc -> <cdi-dir>/psm-<spike>-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

--spike pd is the PVR-DMA download proof (pd_spike.c; it has no host test of its own). --spike hen is the hens-eyes entry (hen_spike.c: hen_eyes.h against hen_sweep.h in the room). --spike ao4 and --spike hen make the room's data first, into <out>/room: the room of demo/psm/ao/room (or of --room; --placeholder-room
for the stand-in) through room_import.py and tools/room_bake.c, and the town's hen through demo/psm/hero/tools/hero_hens.py
from --town-mesh.

--room-only writes the room's data alone (room_mesh.h, room_ao.h) into --out for a program that includes room_data.h: the
room of --room, or the placeholder room without it. It needs only the host compiler.

--host-only stops after step 1 (no KallistiOS needed). --elf-only stops after step 2 (no mkdcdisc needed).
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))


def run(step, argv):
    print("== %s: %s" % (step, " ".join(argv)), flush=True)
    r = subprocess.run(argv, cwd=ROOT)
    if r.returncode:
        raise SystemExit("%s failed (rc %d)" % (step, r.returncode))


def find_kos_cc():
    """kos-cc is a shell script: a Windows Python does not find or start it by name, so look for the file and run it with sh."""
    for d in os.environ.get("PATH", "").split(os.pathsep):
        p = os.path.join(d, "kos-cc")
        if os.path.isfile(p):
            return [shutil.which("sh") or "sh", p] if os.name == "nt" else [p]
    return None


def room_data(a, out):
    """room_mesh.h (room_import.py) and room_ao.h (tools/room_bake.c, on the host) into `out`: what room_data.h includes."""
    exe = ".exe" if os.name == "nt" else ""
    run("room: self-test", [sys.executable, os.path.join(HERE, "room_import.py"), "--self-test"])
    run("room: import", [sys.executable, os.path.join(HERE, "room_import.py")] + (["--room", a.room] if a.room else ["--placeholder"]) + ["--out", out])
    bake = os.path.join(out, "room_bake" + exe)
    run("room: bake (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + out, os.path.join(HERE, "tools", "room_bake.c"), "-o", bake, "-lm"])
    run("room: bake (run)", [bake, out])


def main():
    ap = argparse.ArgumentParser(description="Build the AO images, or the room's data.")
    ap.add_argument("--spike", choices=("ao0", "ao4", "hen", "pd", "lamp", "d1", "mip"), default="ao0")
    ap.add_argument("--tag")
    ap.add_argument("--room-only", action="store_true", help="only the room's data: room_mesh.h and room_ao.h into --out")
    ap.add_argument("--room", default=None, help="the room's folder (room_manifest.json); without it, the placeholder room")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_ao"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--town-inc", action="append", default=[])
    ap.add_argument("--placeholder-room", action="store_true", help="ao4: the importer's stand-in room, not demo/psm/ao/room")
    ap.add_argument("--town-mesh", default=os.path.join(ROOT, "build", "psm_town", "psm_town_mesh.h"), help="ao4: the town's generated mesh, for the hen")
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    if a.room_only:
        room_data(a, out)
        return 0
    if not a.tag:
        ap.error("--tag is needed")
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-%s-%s.cdi" % (a.spike, a.tag))
    elf = os.path.join(out, "psm-%s-%s.elf" % (a.spike, a.tag))
    if not a.host_only and not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    exe = ".exe" if os.name == "nt" else ""
    lib_inc = ["-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "sh4dow-dc", "include")]
    shz_inc = ["-I" + os.path.join(a.sh4zam, "include"), "-I" + os.path.join(ROOT, "third_party", "sh4zam-dce", "include")]
    host = [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DSH4DER_CORE_NO_SH4ZAM"] + lib_inc
    room_inc = []
    if a.spike in ("ao4", "hen", "lamp", "d1"):   # the room's data and the town's hen, into <out>/room
        room = os.path.join(out, "room")
        if not a.room and not a.placeholder_room and os.path.isfile(os.path.join(HERE, "room", "export", "room_manifest.json")):
            a.room = os.path.join(HERE, "room")
        room_data(a, room)
        if not os.path.isfile(a.town_mesh):
            raise SystemExit("no %s: the hen is the town's (tools/bake_town.py makes it; or give --town-mesh)" % a.town_mesh)
        run("room: the town's hen", [sys.executable, os.path.join(ROOT, "demo", "psm", "hero", "tools", "hero_hens.py"), a.town_mesh, room])
        room_inc = ["-I" + room, "-DAO4_TEST_ROOM"]
    # the lamp look draws its halo with the library's ring glow: sh4der_bloom.c and what it calls, on the host and in the ELF
    lamp_src = [os.path.join(ROOT, "src", n) for n in ("sh4der_bloom.c", "sh4der_blade.c", "sh4der_core.c")] if a.spike == "lamp" else []
    for name in ("ao_layer_test", {"ao0": "ao0_model_test", "ao4": "ao4_scene_test", "hen": "hen_room_test", "pd": "ao_layer_test", "lamp": "ao4_lamp_test", "d1": "d1_model_test", "mip": "mip_model_test"}[a.spike]):
        test = os.path.join(out, name + exe)
        run("1 host test %s (compile)" % name, host + (room_inc if name in ("ao4_scene_test", "hen_room_test", "ao4_lamp_test") else []) + [os.path.join(HERE, "test", name + ".c")] +
            (lamp_src if name == "ao4_lamp_test" else []) + ["-o", test, "-lm"])
        run("1 host test %s (run)" % name, [test])
    if a.spike in ("lamp", "d1"):           # the divider's maths and the label lines
        for name, src, inc in (("split_test", os.path.join(ROOT, "test", "split_test.c"), os.path.join(ROOT, "include")),
                               ("psm_effects_test", os.path.join(HERE, "..", "test", "psm_effects_test.c"), os.path.join(HERE, ".."))):
            test = os.path.join(out, name + exe)
            run("1 host test %s (compile)" % name, [a.host_cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + inc, src, "-o", test])
            run("1 host test %s (run)" % name, [test])
    if a.town_inc:
        test = os.path.join(out, "ao_layer_test_town" + exe)
        run("1 host test with the town's sink (compile)", host + ["-DPSM_HAVE_TOWN", "-DAO_TEST_TOWN"] + ["-I" + d for d in a.town_inc] +
            [os.path.join(HERE, "test", "ao_layer_test.c"), "-o", test, "-lm"])
        run("1 host test with the town's sink (run)", [test])
    if a.host_only:
        return 0

    kos_cc = find_kos_cc()
    if not os.environ.get("KOS_BASE") or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if not os.path.isfile(os.path.join(a.sh4zam, "include", "sh4zam", "shz_sh4zam.h")):
        raise SystemExit("no SH4ZAM at %s (see AGENTS.md, or give --sh4zam)" % a.sh4zam)
    lib = os.path.join(a.sh4zam, "build", "libsh4zam.a")
    run("2 elf", kos_cc + ["-O3", "-Wall", "-Wextra", "-ffast-math", "-ffp-contract=fast", "-mfsrra", "-mfsca", "-fno-math-errno"] + lib_inc + shz_inc + room_inc[:1] +
        ["-D%s_TAG=%s" % (a.spike.upper(), a.tag), os.path.join(HERE, "%s_spike.c" % a.spike)] + lamp_src + ([lib] if os.path.isfile(lib) else []) + ["-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("3 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-%s" % a.spike.upper()])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
