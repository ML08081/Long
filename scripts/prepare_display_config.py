#!/usr/bin/env python3
"""Run on build host: merge only display into a freshly downloaded board config."""
import hashlib
import json
from pathlib import Path
import sys

if len(sys.argv) != 4:
    raise SystemExit("usage: prepare_display_config.py BOARD_CONFIG PROFILE OUTPUT")
current, profile, output = map(Path, sys.argv[1:])
raw = current.read_bytes()
old = json.loads(raw.decode("utf-8-sig"))
new = dict(old)
new["display"] = json.loads(profile.read_text(encoding="utf-8-sig"))["display"]
assert new["display"]["controller"] == "ST7735S"
assert all(new[k] == v for k, v in old.items() if k != "display")
output.write_text(json.dumps(new, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
print("Original config SHA256:", hashlib.sha256(raw).hexdigest())
print("Preserved sections:", ", ".join(k for k in old if k != "display"))
