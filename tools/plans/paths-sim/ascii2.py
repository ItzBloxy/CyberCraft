"""Text map of one level: each column's reached place on the level (floor lo..hi) as a letter, a quarter block a
letter from the level's base (m = base), or the kind of the cell at base+1 where nothing on the level is reached:
# blocked, t fence, f floor, . none. Rows are z (north at the top)."""
import sys, math
sys.path.insert(0, ".")
from simlib import *
path = sys.argv[1]
s = tuple(int(a) for a in sys.argv[2:5])
x0, x1, z0, z1 = (int(a) for a in sys.argv[5:9])
lo, hi, h = float(sys.argv[9]), float(sys.argv[10]), int(sys.argv[11])
w = World2(load(path), height=h)
reach = w.flood(s, 45, (s[0], s[2]))
base = w.floor_level(s)
best = {}
for p in reach:
    f = w.floor_level(p)
    if lo <= f <= hi:
        k = (p[0], p[2])
        if k not in best or f > best[k]:
            best[k] = f
print("      " + "".join(str(abs(x) % 10) for x in range(x0, x1 + 1)))
for z in range(z0, z1 + 1):
    row = ""
    for x in range(x0, x1 + 1):
        if (x, z) == (s[0], s[2]):
            row += "V"
        elif (x, z) in best:
            i = int(round((best[(x, z)] - base) * 4)) + 12
            row += chr(ord("a") + max(0, min(25, i)))
        else:
            k = w.kind((x, s[1] + 1, z))[0]
            row += {"blocked": "#", "thin": "t", "floor": "f", "none": "."}[k]
    print(f"{z:6d}" + row)
