#!/usr/bin/env python3
"""Forward: this tool lives in tools/check_credits.py since 2026-11-26 (the library's tools). Imports and command lines that
name this path still work: the names are the library file's, and running this runs it. With no arguments it
checks demo/pl4nar, as it did here."""
import importlib.util
import os
import runpy
import sys

_LIB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "tools", "check_credits.py")

if __name__ == "__main__":
    if len(sys.argv) == 1:
        sys.argv += ["--root", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")]
    sys.argv[0] = _LIB
    runpy.run_path(_LIB, run_name="__main__")
else:
    _spec = importlib.util.spec_from_file_location("sh4der_tools_check_credits", _LIB)
    _mod = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(_mod)
    globals().update({k: v for k, v in vars(_mod).items() if not k.startswith("__")})
