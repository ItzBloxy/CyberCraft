"""Text map of what a mob reaches from V: each column's highest reached floor, as tenths... a letter per quarter block
relative to V's cell's floor (a = -3, ..., m = 0, ..., z = +3.25), or the cell above the road's kind where nothing's
reached: # blocked, t fence, f floor, . none. Rows are z (north at the top), columns x."""
import sys, math
sys.path.insert(0, ".")
from simlib import *
path, fx, fy, fz = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
x0, x1, z0, z1 = (int(a) for a in sys.argv[5:9])
w = World2(load(path))
vx, vy, vz = math.floor(fx), math.floor(fy), math.floor(fz)
s = w.start(vx, vz, vy)
reach = w.flood(s, 30, (vx, vz))
base = w.floor_level(s)
best = {}
for p in reach:
    f = w.floor_level(p)
    k = (p[0], p[2])
    if k not in best or f > best[k]:
        best[k] = f
print(f"start {s} floor {base}; reached {len(reach)}; letters: m = {base}, each letter a quarter block")
print("      " + "".join(str(abs(x) % 10) for x in range(x0, x1 + 1)))
for z in range(z0, z1 + 1):
    row = ""
    for x in range(x0, x1 + 1):
        if (x, z) == (vx, vz):
            row += "V"
        elif (x, z) in best:
            i = int(round((best[(x, z)] - base) * 4)) + 12
            row += chr(ord("a") + max(0, min(25, i))) if i >= 0 else "-"
        else:
            k = w.kind((x, vy + 1, z))[0]
            row += {"blocked": "#", "thin": "t", "floor": "f", "none": "."}[k]
    print(f"{z:6d}" + row)
