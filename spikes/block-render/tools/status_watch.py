"""Watch BlockRender's status in the game: every interval, the time, how long ago the plugin last wrote status.txt, the
status lines whose key matches a pattern, and CET's frame time from the overlay's frames.txt. A status that stops
getting younger means the game stopped updating.

    uv run --no-project python tools/status_watch.py <seconds> [interval] [keys]

The interval is in seconds, 1 by default. The keys are a regular expression, "frame|processes" by default. It reads
the overlay's folder in the game, E:\\Cyberpunk2077\\bin\\x64\\plugins\\cyber_engine_tweaks\\mods\\blockrender_hud.
"""
import re
import sys
import time
from pathlib import Path

HUD = Path("E:/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud")


def read_lines(path):
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None  # missing, or being replaced right now


def sample(folder, keys, now):
    status = folder / "status.txt"
    try:
        out = [f"status written {now - status.stat().st_mtime:.1f} s ago"]
    except OSError:
        return ["status.txt missing"]
    pattern = re.compile(rf"(?:{keys})=")
    out += [line for line in read_lines(status) or [] if pattern.match(line)]
    out += (read_lines(folder / "frames.txt") or [])[:1]
    return out


def main(argv):
    seconds = float(argv[1])
    interval = float(argv[2]) if len(argv) > 2 else 1.0
    keys = argv[3] if len(argv) > 3 else "frame|processes"
    start = time.time()
    while True:
        now = time.time()
        print(f"[{now - start:5.1f} s] " + " | ".join(sample(HUD, keys, now)), flush=True)
        if now + interval - start > seconds:
            break
        time.sleep(interval)


if __name__ == "__main__":
    main(sys.argv)
