"""Walkable places near a level, grouped into the areas a mob can walk between, largest first."""
import sys
sys.path.insert(0, ".")
from simlib import *
from collections import Counter

path, cx, cy, cz, radius = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
lo, hi = float(sys.argv[6]), float(sys.argv[7])
w = World2(load(path))
nodes = set()
for x in range(cx - radius, cx + radius + 1):
    for z in range(cz - radius, cz + radius + 1):
        for y in range(int(lo) - 1, int(hi) + 2):
            p = (x, y, z)
            if w.node_type(p) == WALKABLE and lo <= w.floor_level(p) <= hi:
                nodes.add(p)
left = set(nodes)
groups = []
while left:
    s = left.pop()
    g = w.flood(s, radius, (cx, cz))
    left -= g
    groups.append((s, g))
groups.sort(key=lambda t: -len(t[1]))
for s, g in groups[:12]:
    xs = [p[0] for p in g]; zs = [p[2] for p in g]
    fl = Counter(round(w.floor_level(p), 3) for p in g)
    print(f"{len(g):5d} places from {s}: x {min(xs)}..{max(xs)}, z {min(zs)}..{max(zs)}, floors {fl.most_common(4)}")
