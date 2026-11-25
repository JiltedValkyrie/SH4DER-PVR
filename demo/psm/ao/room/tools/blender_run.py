#!/usr/bin/env python3
"""SH4DER-PVR: run a Python file inside a Blender that is already open (its MCP add-on's local socket).

demo/psm/ao/room/tools/blender_run.py
Copyright (c) 2026 Jilted Valkyrie. See ../../../../../LICENSE.

The add-on listens on 127.0.0.1:9876. A request is one JSON object followed by a zero byte:
    {"type": "execute", "code": "<python>", "strict_json": false}
and the reply is one JSON object followed by a zero byte (status, stdout, message). The code runs in Blender, so bpy
is there. This file is the protocol only; nothing of the add-on is copied.

    python -I demo/psm/ao/room/tools/blender_run.py file.py [--pre "python run before the file"]

Exit 1 when Blender reports an error or does not answer.
"""
import json
import socket
import sys


def run(code, host="127.0.0.1", port=9876, timeout=1800.0):
    with socket.create_connection((host, port), timeout=timeout) as s:
        s.sendall(json.dumps({"type": "execute", "code": code, "strict_json": False}).encode("utf-8") + b"\0")
        buf = b""
        while not buf.endswith(b"\0"):
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
    return json.loads(buf.rstrip(b"\0").decode("utf-8")) if buf.strip(b"\0") else {"status": "error", "message": "no reply"}


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    pre = argv[argv.index("--pre") + 1] + "\n" if "--pre" in argv else ""
    with open(argv[1], encoding="utf-8") as f:
        reply = run(pre + f.read())
    out = reply.get("stdout") or reply.get("result") or ""
    if out:
        print(out if isinstance(out, str) else json.dumps(out, indent=1))
    if reply.get("status") not in ("success", "ok"):
        print("BLENDER ERROR:", reply.get("message") or reply)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
