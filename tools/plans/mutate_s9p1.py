"""Spike 9 Part 1's mutations of CellRule: each must make test.sh fail with the named check."""
import shutil, subprocess, sys
from pathlib import Path
T = Path(sys.argv[1])  # the cybercraft-tweaks folder
SRC = T / "src/main/java/dev/cybercraft/tweaks/paths/CellRule.java"
MUTATIONS = [
    ("top <= 4 ? FLOOR", "top <= 3 ? FLOOR", "a skin at layer 3 is a floor at its top"),
    ("for (int z = 1; z < 7; z++)", "for (int z = 0; z < 8; z++)", "walls along the edges leave the middle empty"),
    ("for (int x = 1; x < 7; x++)", "for (int x = 0; x < 7; x++)", "walls along the edges leave the middle empty"),
    ("top == 0 ? NONE", "top <= 1 ? NONE", "a skin at layer 0 is a floor at its top"),
    ("return y - 1 + top(below);", "return y + top(below);", "above a blocked cell, on its top"),
    ("if (below >= 0 && kind(below) != NONE) {", "if (below >= 0 && kind(below) == BLOCKED) {", "above a floor cell, on its floor"),
    ("|| below >= 0 && kind(below) == BLOCKED;", "|| below >= 0 && kind(below) != NONE;", "above a floor cell isn't a stroll target"),
    ("== SEARCH_FLOOR ? below : below + 1;", "== SEARCH_FLOOR ? below + 1 : below + 1;", "from air, into the floor cell below"),
    ("            y = above;\n", "", "nothing below: up while air, then up out of the solid"),
    ("            return above;\n        }\n        return y;", "            return y;\n        }\n        return y;", "inside something solid: up out of it"),
    ("(int) Math.ceil(", "(int) Math.round(", "8.5 blocks: two pieces meeting halfway"),
    ("points[count] = to.clone();", "points[count] = from.clone();", "the pieces start and end at the ends"),
]
original = SRC.read_text(encoding="utf-8")
caught = 0
try:
    for old, new, check in MUTATIONS:
        assert original.count(old) == 1, old
        SRC.write_text(original.replace(old, new), encoding="utf-8")
        r = subprocess.run(["bash", "test.sh"], cwd=T, capture_output=True, text=True)
        ok = r.returncode != 0 and f"FAIL: {check}" in r.stdout
        caught += ok
        print(("caught " if ok else "MISSED ") + f"{old[:40]!r} -> {check}")
finally:
    SRC.write_text(original, encoding="utf-8")
print(f"{caught} of {len(MUTATIONS)} caught")
