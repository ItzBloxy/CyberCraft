"""Spike 9's fix tasks: mutations of CellRule (the fences, and Part 1's rules again); each must fail test.sh with the
named check."""
import subprocess, sys
from pathlib import Path
T = Path(sys.argv[1])
SRC = T / "src/main/java/dev/cybercraft/tweaks/paths/CellRule.java"
MUTATIONS = [
    ("covered < 18 ? THIN", "covered < 17 ? THIN", "one column short of half is a fence"),
    ("covered < 18 ? THIN", "covered < 19 ? THIN", "half the middle covered is a floor"),
    ("below != null && below[i] > 4", "below != null && below[i] > 3", "the cell below's lower half covers nothing"),
    ("tops[i] > 0 || below != null && below[i] > 4", "tops[i] > 0", "a kerb's cell, part pavement and part the street's top below, is a floor"),
    ("(kind(below) == THIN ? FENCE_RISE : 0.0)", "(kind(below) == THIN ? 0.0 : 0.0)", "above a fence, half a block over its top"),
    ("|| below >= 0 && kind(below) == BLOCKED;", "|| below >= 0 && kind(below) >= BLOCKED;", "above a fence isn't a stroll target"),
    ("top == 0 ? NONE : covered < 18 ? THIN", "covered < 18 ? THIN : top == 0 ? NONE", "an empty cell is none"),
    ("int kind = top == 0 ? NONE : covered < 18 ? THIN : top <= 4 ? FLOOR : BLOCKED;", "int kind = top == 0 ? NONE : top <= 4 ? FLOOR : BLOCKED;", "a wall through the middle is a fence"),
    ("top <= 4 ? FLOOR", "top <= 3 ? FLOOR", "a skin at layer 3 is a floor at its top"),
    ("for (int z = 1; z < 7; z++)", "for (int z = 0; z < 8; z++)", "walls along the edges leave the middle empty"),
    ("for (int x = 1; x < 7; x++)", "for (int x = 0; x < 7; x++)", "walls along the edges leave the middle empty"),
    ("return y - 1 + top(below) + (", "return y + top(below) + (", "above a blocked cell, on its top"),
    ("if (below >= 0 && kind(below) != NONE) {", "if (below >= 0 && kind(below) == BLOCKED) {", "above a floor cell, on its floor"),
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
        print(("caught " if ok else "MISSED ") + f"{old[:44]!r} -> {check}")
finally:
    SRC.write_text(original, encoding="utf-8")
print(f"{caught} of {len(MUTATIONS)} caught")
