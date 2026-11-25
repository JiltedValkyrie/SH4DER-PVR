#!/usr/bin/env python3
"""build.py --dry-run: for both builds (the map, the default; --street) it names the steps, reports the tools, runs the
credits check, and touches nothing.

    python demo/pl4nar/tools/test_build_dry.py

No network, no compiler, no Blender: this runs anywhere Python does. Standard library only.
"""
import os
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def tree():
    """Every file under the demo folder with its size and time: what a dry run must leave as it is."""
    out = {}
    for base, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for f in files:
            p = os.path.join(base, f)
            st = os.stat(p)
            out[os.path.relpath(p, ROOT)] = (st.st_size, st.st_mtime_ns)
    return out


def dry(*extra):
    return subprocess.run([sys.executable, os.path.join(ROOT, "build.py"), "--dry-run"] + list(extra), capture_output=True, text=True)


STREET = ("1 credits", "2 fetch", "3 coupe", "4 lod", "5 bake", "6 make", "7 image")
MAP = ("1 credits", "2 fetch", "3 coupe", "3b structures", "4 lod", "5 bake", "5b map", "5c map_ao", "6 make", "7 image")


class DryRun(unittest.TestCase):
    def steps(self, extra, want, image):
        before = tree()
        r = dry("--sh4zam", os.path.join(ROOT, "no-such-checkout"), "--out", os.path.join(ROOT, "no-such-out"), *extra)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        lines = [ln for ln in r.stdout.splitlines() if ln.startswith("DRY_RUN")]
        for step in want:
            self.assertEqual(sum(1 for ln in lines if ln.startswith("DRY_RUN " + step + ":")), 1, step)
        self.assertEqual(sum(1 for ln in lines if ln[8:9].isdigit()), len(want), "no step more than " + ", ".join(want))
        self.assertEqual(lines[-1], "DRY_RUN_OK")
        self.assertIn("CREDITS_OK", r.stdout)
        self.assertIn("SH4ZAM MISSING", r.stdout)                  # a missing tool is reported, not an error
        self.assertIn(image, r.stdout)                             # no image folder yet: the first number
        self.assertEqual(tree(), before)                           # nothing fetched, written or built
        self.assertFalse(os.path.exists(os.path.join(ROOT, "no-such-out")))
        return r.stdout

    def test_map_steps_and_nothing_written(self):
        out = self.steps((), MAP, "pl4nar-map-b1.cdi")
        self.assertIn("the map (default)", out)
        self.assertIn("MAP=1 GEN=build/gen_map BUILD=build/map", out)
        self.assertIn("host cc", out)                              # found or MISSING: either way it is named

    def test_street_steps_and_nothing_written(self):
        out = self.steps(("--street",), STREET, "pl4nar-demo-b1.cdi")
        self.assertIn("the street (--street)", out)
        self.assertNotIn("MAP=1", out)
        self.assertNotIn("host cc", out)

    def test_map_without_host_cc_refuses_in_the_list(self):
        env = dict(os.environ, HOST_CC="no-such-host-cc")
        r = subprocess.run([sys.executable, os.path.join(ROOT, "build.py"), "--dry-run"], capture_output=True, text=True, env=env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)    # reported, not an error, in a dry run
        self.assertIn("5c map_ao: REFUSES: no host C compiler", r.stdout)

    def test_real_run_without_host_cc_refuses_first(self):
        before = tree()
        env = dict(os.environ, HOST_CC="no-such-host-cc")
        r = subprocess.run([sys.executable, os.path.join(ROOT, "build.py"), "--sh4zam", os.path.join(ROOT, "no-such-checkout")],
                           capture_output=True, text=True, env=env)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("no host C compiler", r.stdout + r.stderr)
        self.assertEqual(tree(), before)                           # refused before anything ran

    def test_flags_show(self):
        for extra in ((), ("--street",)):
            r = dry("--rtt", "--pin", *extra)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("RTT=1", r.stdout)
            self.assertNotIn("would need --pin", r.stdout)
            r = dry(*extra)
            self.assertIn("no RTT", r.stdout)

    def test_fetch_table_rows(self):
        r = dry()
        row = [ln for ln in r.stdout.splitlines() if ln.startswith("DRY_RUN 2 fetch")][0]
        self.assertIn("8 packs", row)


if __name__ == "__main__":
    unittest.main()
