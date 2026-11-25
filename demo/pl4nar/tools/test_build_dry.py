#!/usr/bin/env python3
"""build.py --dry-run: it names the seven steps, reports the tools, runs the credits check, and touches nothing.

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


class DryRun(unittest.TestCase):
    def test_steps_and_nothing_written(self):
        before = tree()
        r = dry("--sh4zam", os.path.join(ROOT, "no-such-checkout"), "--out", os.path.join(ROOT, "no-such-out"))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        lines = [ln for ln in r.stdout.splitlines() if ln.startswith("DRY_RUN")]
        for step in ("1 credits", "2 fetch", "3 coupe", "4 lod", "5 bake", "6 make", "7 image"):
            self.assertEqual(sum(1 for ln in lines if ln.startswith("DRY_RUN " + step)), 1, step)
        self.assertEqual(lines[-1], "DRY_RUN_OK")
        self.assertIn("CREDITS_OK", r.stdout)
        self.assertIn("SH4ZAM MISSING", r.stdout)                  # a missing tool is reported, not an error
        self.assertIn("pl4nar-demo-b1.cdi", r.stdout)              # no image folder yet: the first number
        self.assertEqual(tree(), before)                           # nothing fetched, written or built
        self.assertFalse(os.path.exists(os.path.join(ROOT, "no-such-out")))

    def test_flags_show(self):
        r = dry("--rtt", "--pin")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("RTT=1", r.stdout)
        self.assertNotIn("would need --pin", r.stdout)
        r = dry()
        self.assertIn("no RTT", r.stdout)

    def test_fetch_table_rows(self):
        r = dry()
        row = [ln for ln in r.stdout.splitlines() if ln.startswith("DRY_RUN 2 fetch")][0]
        self.assertIn("8 packs", row)


if __name__ == "__main__":
    unittest.main()
