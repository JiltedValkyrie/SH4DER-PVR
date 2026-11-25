"""Reads config/config.local (KEY=value lines). Machine paths live there only, never in committed text."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def load():
    cfg = {}
    path = os.path.join(ROOT, "config", "config.local")
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                cfg[k.strip()] = v.strip()
    for k in list(cfg):
        cfg[k] = os.environ.get(k, cfg[k])
    return cfg

def need(cfg, key):
    if key not in cfg:
        raise SystemExit("config key %s missing: see config/config.example.cfg" % key)
    return cfg[key]
