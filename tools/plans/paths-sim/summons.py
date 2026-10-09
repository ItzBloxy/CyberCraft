"""Where the Paths row's summons land from V standing in an area facing a heading: each mob goes from the player's feet
+ 0.5 (+ 1 for the bee) at its place ahead (^left ^ ^ahead), and drops to the first floor below. A spot counts when
every landing is in the area V's in (two-tall mobs' area; the wolf's and the pig's one-tall), and so are the places a
block either side of each landing, against a few degrees of heading."""
import sys, math
sys.path.insert(0, ".")
from simlib import *
binf = sys.argv[1]
start = tuple(int(a) for a in sys.argv[2:5])
feet_dy = float(sys.argv[5])  # V's feet below the floor's voxel top, as in the game (the triangles' surface)
cells = load(binf)
w2 = World2(cells, height=2)
w1 = World2(cells, height=1)
area2 = w2.flood(start, 40, (start[0], start[2]))
area1 = w1.flood(start, 40, (start[0], start[2]))
cols2, cols1 = {}, {}
for p in area2:
    cols2.setdefault((p[0], p[2]), []).append(p)
for p in area1:
    cols1.setdefault((p[0], p[2]), []).append(p)
ROW = {"pig": (2, 4, 0.5, 1), "cow": (0, 4, 0.5, 2), "villager": (-2, 4, 0.5, 2), "wolf": (2, 2, 0.5, 1),
       "bee": (-2, 2, 1.0, 0), "husk": (0, 7, 0.5, 2)}
def lands(x, z, top, cols):
    c = cols.get((math.floor(x), math.floor(z)))
    if not c:
        return None
    below = [p for p in c if w2.floor_level(p) <= top + 1e-6]
    return max(below, key=lambda p: w2.floor_level(p)) if below else None
good = []
for p in sorted(area2):
    floor = w2.floor_level(p)
    fx, fz, fy = p[0] + 0.5, p[2] + 0.5, floor - feet_dy
    for deg in range(0, 360, 15):
        f = (math.sin(math.radians(deg)), -math.cos(math.radians(deg)))
        left = (-math.cos(math.radians(deg)), -math.sin(math.radians(deg)))
        ok = True
        where = {}
        for name, (l, a, up, h) in ROW.items():
            x, z = fx + left[0] * l + f[0] * a, fz + left[1] * l + f[1] * a
            cols = cols1 if h == 1 else cols2
            for ex, ez in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
                if name == "bee":
                    if not cols2.get((math.floor(x + ex), math.floor(z + ez))) and not cols1.get((math.floor(x + ex), math.floor(z + ez))):
                        ok = False
                    continue
                q = lands(x + ex, z + ez, fy + up, cols)
                if q is None or abs(w2.floor_level(q) - floor) > 1.2:
                    ok = False
            where[name] = (math.floor(x), math.floor(z))
            if not ok:
                break
        if ok:
            good.append((p, deg, where))
print(len(good), "spots")
for g in good[:: max(1, len(good) // 25)]:
    print(g)
