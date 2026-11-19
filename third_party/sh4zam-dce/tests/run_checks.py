"""SH4ZAM-DCE: build and run every check of the library.

  python run_checks.py [--repo <repository root>] [--build <build folder>] [--only topic[,topic]] [--hw-model auto|off|<folder>] [step ...]

A topic is one header, include/sh4zam_dce/shz_dce_<topic>.h, with its files under tests/:
  host/<topic>_test.c     host test on SH4ZAM's software back end; prints "<topic>_test OK"
  sh4/<topic>_kernels.c   every routine of the header called from SH-4 code (built by sh-elf-gcc)
  sh4/<topic>_check.c     host program: runs that SH-4 code in the interpreter and checks it; prints "<topic>_check OK"
  sh4/<topic>_bench.c     SH-4 loops (functions named bench_*) for the cycle ESTIMATE table

Steps (default: all, in this order):
  selftest   the SH-4 interpreter's own test (tests/sh4run): decoder against objdump, native against interpreted, the faults it must catch
  host       host/<topic>_test.c with strict IEEE flags and with -ffast-math -ffp-contract=fast
  compile    sh-elf-gcc -ml -m4-single, -O2 and -O3, strict and fast-math flags, -Wall -Wextra -Werror: every header alone, every
             <topic>_kernels.c and <topic>_bench.c
  sh4        <topic>_kernels.c in the four flag sets, each run by <topic>_check through the interpreter
  cycles     <topic>_bench.c with the fast-math flags at -O2 and -O3 through sh4bench: <build>/cycles_<topic>.txt

The toolchain is found from the repository's own configuration: KOS_CC_BASE and MINGW_BIN from the environment or <repo>/config/config.local,
the host compiler from CC, <repo>/tools/bin/cc-venv/Scripts or PATH. No path is written in this file. Every compiler and test runs
hidden (no console window), below normal priority, one at a time; its output goes to <build>/logs.

--hw-model: auto (the default: tests/third_party/sh4_fpu_approx when it is there), off, or the folder of sh4_fpu_approx.h (bit-exact
models of FIPR / FTRV / FSRRA / FSCA, a third party's, MIT). With it the interpreter gives the console's own bits for those four
instructions; without it, plain sums.

--repo defaults to the repository this folder sits in (third_party/sh4zam-dce/tests), --build to <repo>/build/sh4zam-dce.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIB = HERE.parent
NO_WINDOW = 0x08000000 if os.name == "nt" else 0
BELOW_NORMAL = 0x00004000 if os.name == "nt" else 0
STRICT = ["-fno-fast-math", "-ffp-contract=off", "-fno-math-errno"]
FAST = ["-ffast-math", "-ffp-contract=fast", "-fno-math-errno"]
SH4_FAST = FAST + ["-mfsrra", "-mfsca"]
SH4_STRICT = STRICT + ["-mfsrra", "-mfsca"]
SH4_SETS = [("O2_strict", "-O2", SH4_STRICT), ("O2_fast", "-O2", SH4_FAST), ("O3_strict", "-O3", SH4_STRICT), ("O3_fast", "-O3", SH4_FAST)]
TEXT_BASE = "0x8c010000"
# objdump mnemonics the interpreter does not run (privileged, GBR byte logic, SH-4A, MAC): in code they stop the run, in a literal pool they are data
NOT_MODELLED = {"stc", "ldc", "stc.l", "ldc.l", "tst.b", "and.b", "or.b", "xor.b", "tas.b", "mac.l", "mac.w", "trapa", "rte", "sleep", "ldtlb",
                "movco.l", "movli.l", "movua.l", "icbi", "prefi", "synco", "fpchg", "ldrs", "ldre", "ldrc", "setrc", "repeat", "pabs", "pabs.l"}


class Env:
    def __init__(self, args):
        self.repo = Path(args.repo).resolve()
        self.build = Path(args.build).resolve()
        self.logs = self.build / "logs"
        self.logs.mkdir(parents=True, exist_ok=True)
        hw = getattr(args, "hw_model", None) or "auto"
        vendored = HERE / "third_party" / "sh4_fpu_approx"
        if hw == "off":
            self.hw_model = None
        elif hw == "auto":
            self.hw_model = vendored if (vendored / "sh4_fpu_approx.h").is_file() else None
        else:
            self.hw_model = Path(hw).resolve()
        only = getattr(args, "only", None)
        self.only = set(only.split(",")) if only else None
        cfg = {}
        local = self.repo / "config" / "config.local"
        if local.is_file():
            for line in local.read_text(encoding="utf-8", errors="replace").splitlines():
                m = re.match(r"\s*([A-Z0-9_]+)\s*=\s*(.+?)\s*$", line)
                if m:
                    cfg[m.group(1)] = m.group(2)
        self.kos_cc = os.environ.get("KOS_CC_BASE") or cfg.get("KOS_CC_BASE")
        self.mingw = os.environ.get("MINGW_BIN") or cfg.get("MINGW_BIN")
        venv = self.repo / "tools" / "bin" / "cc-venv" / "Scripts"
        cc = os.environ.get("CC")
        if not cc and (venv / "gcc.exe").is_file():
            cc = str(venv / "gcc.exe")
        self.cc = cc or shutil.which("gcc")
        self.env = dict(os.environ, LC_ALL="C")
        extra = [str(venv)] if venv.is_dir() else []
        if self.mingw:
            extra.append(self.mingw)
        if self.kos_cc:
            extra.append(str(Path(self.kos_cc) / "bin"))
        self.env["PATH"] = os.pathsep.join(extra + [self.env.get("PATH", "")])
        self.shz = self.repo / "third_party" / "sh4zam"
        self.failed = []
        self._interp = None
        self._lib = None

    def topics(self, pattern):
        """Topics that have the file tests/<pattern with {} = topic>, narrowed by --only."""
        folder, name = pattern.split("/")
        found = sorted(p.name[:-len(name) + 2] for p in (HERE / folder).glob(name.replace("{}", "*")))
        return [t for t in found if not self.only or t in self.only]

    def sh(self, tool):
        return str(Path(self.kos_cc) / "bin" / ("sh-elf-" + tool + (".exe" if os.name == "nt" else "")))

    def run(self, name, cmd, cwd=None, timeout=1800):
        """Run hidden; the output goes to logs/<name>.log. Returns (exit code, output)."""
        log = self.logs / (name + ".log")
        try:
            p = subprocess.run([str(c) for c in cmd], cwd=cwd, env=self.env, capture_output=True, text=True, errors="replace", timeout=timeout,
                               creationflags=NO_WINDOW | BELOW_NORMAL)
            out, rc = p.stdout + p.stderr, p.returncode
        except subprocess.TimeoutExpired as e:
            out, rc = f"TIMEOUT after {timeout} s\n{e.stdout or ''}{e.stderr or ''}", 124
        log.write_text(" ".join(str(c) for c in cmd) + "\n\n" + out, encoding="utf-8")
        return rc, out

    def must(self, name, cmd, **kw):
        rc, out = self.run(name, cmd, **kw)
        if rc != 0:
            self.failed.append(name)
            print(f"FAIL {name} (exit {rc}); log: {self.logs / (name + '.log')}")
            print("\n".join(out.strip().splitlines()[-25:]))
        return rc == 0, out

    def expect(self, name, cmd, ok_line, **kw):
        """Run a test program: exit 0 and its OK line. Prints its other lines (the measured counts)."""
        ok, out = self.must(name, cmd, **kw)
        if ok and ok_line not in out:
            self.failed.append(name)
            print(f"FAIL {name}: no '{ok_line}' line")
            return False
        if ok:
            print(f"  {name}: " + " | ".join(l for l in out.splitlines() if l.strip() and not l.endswith(" OK"))[:1000])
        return ok

    def host_flags(self):
        return ["-std=gnu17", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(LIB / "include"), "-I" + str(self.shz / "include"), "-I" + str(HERE),
                "-I" + str(HERE / "sh4run"), "-I" + str(HERE / "sh4")]

    def sh4_cc(self, opt, flags):
        return [self.sh("gcc"), "-ml", "-m4-single", opt, "-Wall", "-Wextra", "-Werror", "-fomit-frame-pointer", *flags,
                "-I" + str(LIB / "include"), "-I" + str(self.shz / "include"), "-I" + str(HERE / "sh4")]

    def hw_flags(self):
        return ["-DSH4RUN_HW_MODEL", "-I" + str(self.hw_model)] if self.hw_model else []

    def interp(self):
        """sh4run.c built for the host: strict IEEE, fmaf as one instruction, the bit-exact unit models when given."""
        if self._interp is None:
            out = self.build / "sh4run.o"
            cmd = [self.cc, "-std=gnu17", "-O2", "-Wall", "-Wextra", "-Werror", "-mfma", *STRICT, "-I" + str(HERE / "sh4run"), *self.hw_flags()]
            ok, _ = self.must("sh4run_cc", cmd + ["-c", str(HERE / "sh4run" / "sh4run.c"), "-o", str(out)])
            self._interp = out if ok else False
        return self._interp or None

    def tool(self, name):
        """A host tool of tests/sh4run (sh4run_cli, sh4bench) linked with the interpreter."""
        exe = self.build / (name + ".exe")
        d = HERE / "sh4run"
        if not self.interp():
            return None
        ok, _ = self.must(name + "_cc", [self.cc, "-std=gnu17", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(d), str(d / (name + ".c")), str(self.interp()),
                                         "-lm", "-o", str(exe)])
        return exe if ok else None

    def sh4zam_lib(self):
        """SH4ZAM's own compiled half (the few routines that are not inline) and the test programs' assert, built into the build
        folder with SH4ZAM's makefile flags. SH4ZAM's tree is only read."""
        if self._lib is None:
            self._lib = []
            src = self.shz / "source"
            files = [src / rel for rel in ("sh4/shz_complex_sh4.c", "sh4/shz_matrix_sh4.c", "shz_version.c", "sh4/shz_mem_sh4.s", "sh4/shz_xmtrx_sh4.s")]
            files.append(HERE / "sh4run" / "sh4_support.c")
            for f in files:
                if not f.is_file():
                    continue
                o = self.build / ("shz_lib_" + f.name + ".o")
                cmd = [self.sh("gcc"), "-ml", "-m4-single", "-O2", "-Wall", "-I" + str(self.shz / "include"), *SH4_FAST, "-c", str(f), "-o", str(o)]
                ok, _ = self.must("shz_lib_" + f.stem, cmd)
                if ok:
                    self._lib.append(o)
        return self._lib

    def sh4_link(self, name, objs):
        elf = self.build / (name + ".elf")
        ok, _ = self.must(name + "_link", [self.sh("gcc"), "-ml", "-m4-single", "-nostartfiles", f"-Wl,-Ttext={TEXT_BASE}", f"-Wl,-e,{TEXT_BASE}",
                                           *[str(o) for o in objs], "-lm", "-lc", "-lgcc", "-o", str(elf)])
        return elf if ok else None


def decode_check(env, name, elf):
    """Every 16-bit word of the ELF's code: this tool's mnemonic against GNU objdump's."""
    cli = env.tool("sh4run_cli")
    if not cli:
        return False
    ok, mine = env.must(name + "_mn", [cli, "--mn", elf])
    ok2, dump = env.must(name + "_objdump", [env.sh("objdump"), "-d", "--no-show-raw-insn", elf])
    if not (ok and ok2):
        return False
    theirs = {}
    for line in dump.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\t(\S+)", line)
        if m:
            theirs[int(m.group(1), 16)] = m.group(2)
    bad, seen, unknown = [], 0, 0
    for line in mine.splitlines():
        addr, mn = line.split()
        want = theirs.get(int(addr, 16))
        if want is None:
            continue
        seen += 1
        if mn == "?" and (want.startswith(".") or want in NOT_MODELLED):
            unknown += 1   # data in a literal pool, or an instruction the interpreter refuses to run: executing one stops the run and says so
            continue
        if mn != want:
            bad.append(f"{addr}: sh4run '{mn}', objdump '{want}'")
    (env.logs / (name + "_decode.log")).write_text(f"{seen} words compared, {len(bad)} differ, {unknown} not modelled\n" + "\n".join(bad), encoding="utf-8")
    if bad or not seen:
        env.failed.append(name + "_decode")
        print(f"FAIL {name} decode: {len(bad)} of {seen} words differ; first: {bad[:5]}")
        return False
    return True


def step_selftest(env):
    d = HERE / "sh4run"
    if not env.interp():
        return
    obj = env.build / "selftest_sh4.o"
    ok, _ = env.must("selftest_sh4_cc", [env.sh("gcc"), "-ml", "-m4-single", "-O2", "-Wall", "-Wextra", *SH4_STRICT, "-I" + str(d), "-I" + str(env.shz / "include"),
                                          "-c", str(d / "selftest_sh4.c"), "-o", str(obj)])
    elf = env.sh4_link("selftest_sh4", [obj]) if ok else None
    if not elf:
        return
    if decode_check(env, "selftest", elf):
        print("  decoder: sh4run and objdump agree on every word of the self-test")
    exe = env.build / "selftest.exe"
    cmd = [env.cc, "-std=gnu17", "-O2", "-Wall", "-Wextra", "-Werror", "-mfma", *STRICT, "-I" + str(d), *env.hw_flags()]
    ok, _ = env.must("selftest_cc", cmd + [str(d / "selftest.c"), str(env.interp()), "-lm", "-o", str(exe)])
    if ok:
        env.expect("selftest_run", [exe, elf], "sh4run selftest: ok")


def step_host(env):
    sw = [str(env.shz / "source" / "sw" / "shz_xmtrx_sw.c")]
    for topic in env.topics("host/{}_test.c"):
        src = HERE / "host" / f"{topic}_test.c"
        for tag, flags in (("strict", STRICT), ("fast", FAST)):
            exe = env.build / f"{topic}_test_{tag}.exe"
            ok, _ = env.must(f"{topic}_test_{tag}_cc", [env.cc, *env.host_flags(), *flags, str(src), *sw, "-lm", "-o", str(exe)])
            if ok:
                env.expect(f"{topic}_test_{tag}", [exe], f"{topic}_test OK", cwd=str(env.repo), timeout=3600)


def step_compile(env):
    headers = [h for h in sorted((LIB / "include" / "sh4zam_dce").glob("*.h"))
               if not env.only or h.stem.replace("shz_dce_", "") in env.only or h.stem in ("shz_dce", "shz_dce_cdefs")]
    sources = [HERE / "sh4" / f"{t}_kernels.c" for t in env.topics("sh4/{}_kernels.c")] + [HERE / "sh4" / f"{t}_bench.c" for t in env.topics("sh4/{}_bench.c")]
    n = total = 0
    for tag, opt, flags in SH4_SETS:
        for h in headers:   # a header must stand on its own
            stub = env.build / f"only_{h.stem}.c"
            stub.write_text(f"#include <sh4zam_dce/{h.name}>\nint only_{h.stem};\n", encoding="utf-8")
            ok, _ = env.must(f"only_{h.stem}_{tag}", env.sh4_cc(opt, flags) + ["-c", str(stub), "-o", str(env.build / f"only_{h.stem}_{tag}.o")])
            n, total = n + ok, total + 1
        for src in sources:
            ok, _ = env.must(f"{src.stem}_{tag}_cc", env.sh4_cc(opt, flags) + ["-c", str(src), "-o", str(env.build / f"{src.stem}_{tag}.o")])
            n, total = n + ok, total + 1
    print(f"  compile: {n} of {total} SH-4 compiles clean (-ml -m4-single, -O2 / -O3, strict / -ffast-math, -Wall -Wextra -Werror)")


def step_sh4(env):
    if not env.interp():
        return
    lib = env.sh4zam_lib()
    for topic in env.topics("sh4/{}_check.c"):
        exe = env.build / f"{topic}_check.exe"
        ok, _ = env.must(f"{topic}_check_cc", [env.cc, *env.host_flags(), "-mfma", *STRICT, *env.hw_flags(), str(HERE / "sh4" / f"{topic}_check.c"), str(env.interp()),
                                               str(env.shz / "source" / "sw" / "shz_xmtrx_sw.c"), "-lm", "-o", str(exe)])
        if not ok:
            continue
        for tag, opt, flags in SH4_SETS:
            obj = env.build / f"{topic}_kernels_{tag}.o"
            ok, _ = env.must(f"{topic}_kernels_{tag}_cc", env.sh4_cc(opt, flags) + ["-c", str(HERE / "sh4" / f"{topic}_kernels.c"), "-o", str(obj)])
            elf = env.sh4_link(f"{topic}_kernels_{tag}", [obj] + lib) if ok else None
            if elf and decode_check(env, f"{topic}_kernels_{tag}", elf):
                env.expect(f"{topic}_check_{tag}", [exe, elf], f"{topic}_check OK", timeout=3600)


def step_cycles(env):
    bench = env.tool("sh4bench")
    if not bench:
        return
    lib = env.sh4zam_lib()
    for topic in env.topics("sh4/{}_bench.c"):
        src = HERE / "sh4" / f"{topic}_bench.c"
        # `void bench_x(` at the start of a line, or a one-line loop macro `F1(bench_x, ...)`
        names = ["_" + n for n in re.findall(r"^(?:void |[A-Z][A-Z0-9]*\()(bench_\w+)[(,]", src.read_text(encoding="utf-8"), re.M)]
        report = [f"SH4ZAM-DCE cycle ESTIMATES, {topic}: the interpreter's SH7750 pipeline model on the compiled SH-4 code, cache hits assumed.",
                  "Not a console measurement. An element is one item of the batch (a point, a sphere, a vertex, a quaternion).", ""]
        for opt in ("-O2", "-O3"):
            tag = f"{topic}_bench_{opt[1:]}"
            obj = env.build / (tag + ".o")
            ok, _ = env.must(tag + "_cc", env.sh4_cc(opt, SH4_FAST) + ["-c", str(src), "-o", str(obj)])
            env.must(tag + "_s", env.sh4_cc(opt, SH4_FAST) + ["-S", str(src), "-o", str(env.build / (tag + ".s"))])
            elf = env.sh4_link(tag, [obj] + lib) if ok else None
            if elf:
                ok, out = env.must(tag + "_run", [bench, elf] + names)
                report += [f"sh-elf-gcc -ml -m4-single {opt} {' '.join(SH4_FAST)}", out]
        (env.build / f"cycles_{topic}.txt").write_text("\n".join(report), encoding="utf-8")
        print("\n".join(report))


STEPS = {"selftest": step_selftest, "host": step_host, "compile": step_compile, "sh4": step_sh4, "cycles": step_cycles}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=None)
    ap.add_argument("--build", default=None)
    ap.add_argument("--only", default=None, help="comma separated topics (default: every topic)")
    ap.add_argument("--hw-model", default="auto")
    ap.add_argument("steps", nargs="*")
    args = ap.parse_args()
    if not args.repo:
        args.repo = str(LIB.parent.parent)
        if not (Path(args.repo) / "third_party" / "sh4zam").is_dir():
            print("--repo is needed: this folder is not inside a repository that has third_party/sh4zam")
            return 2
    if not args.build:
        args.build = str(Path(args.repo) / "build" / "sh4zam-dce")
    env = Env(args)
    if not env.cc:
        print("no host compiler (CC, the repository's cc-venv or gcc on PATH)")
        return 2
    if not env.kos_cc or not Path(env.sh("gcc")).is_file():
        print("no SH-4 compiler (KOS_CC_BASE in the environment or config/config.local)")
        return 2
    for step in (args.steps or list(STEPS)):
        if step not in STEPS:
            print(f"unknown step {step}; steps: {', '.join(STEPS)}")
            return 2
        print(f"== {step}")
        STEPS[step](env)
    if env.failed:
        print(f"FAILED: {', '.join(env.failed)}")
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
