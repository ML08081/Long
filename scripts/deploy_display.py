#!/usr/bin/env python3
"""Run ON the Loongson: back up runtime, merge only display, install and restart.

Usage: python3 deploy_display.py STAGED_BINARY STAGED_PROFILE
The profile is config/config.json; only its display member is applied.
"""
import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    staged, profile = map(Path, sys.argv[1:])
    target = Path("/usr/bin/patrol_system")
    config = Path("/etc/patrol/config.json")
    old = json.loads(config.read_text(encoding="utf-8-sig"))
    new = dict(old)
    new["display"] = json.loads(profile.read_text(encoding="utf-8-sig"))["display"]
    assert new["display"]["controller"] == "ST7735S"
    assert all(new[k] == v for k, v in old.items() if k != "display")
    # Never replace a service binary unless the supplied executable can report its version.
    version = subprocess.check_output([str(staged), "--version"], text=True)
    if "1.24.0" not in version:
        raise RuntimeError("Unexpected staged version: " + version)
    backup = Path("/root/patrol-display-backups") / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup.mkdir(parents=True, exist_ok=False)
    shutil.copy2(target, backup / "patrol_system")
    shutil.copy2(config, backup / "config.json")
    subprocess.run(["systemctl", "stop", "patrol"], check=True)
    try:
        tempbin = target.with_name("patrol_system.display-new")
        shutil.copy2(staged, tempbin)
        tempbin.chmod(0o755)
        tempcfg = config.with_name("config.json.display-new")
        tempcfg.write_text(json.dumps(new, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        shutil.copymode(config, tempcfg)
        os.replace(tempbin, target)
        os.replace(tempcfg, config)
        subprocess.run(["systemctl", "start", "patrol"], check=True)
        time.sleep(5)
        subprocess.run(["systemctl", "is-active", "--quiet", "patrol"], check=True)
    except BaseException:
        subprocess.run(["systemctl", "stop", "patrol"], check=False)
        shutil.copy2(backup / "patrol_system", target)
        shutil.copy2(backup / "config.json", config)
        subprocess.run(["systemctl", "start", "patrol"], check=False)
        raise
    print("Version:", version.strip())
    print("Backup:", backup)
    print("Non-display configuration preserved:", ", ".join(k for k in old if k != "display"))


if __name__ == "__main__":
    main()
