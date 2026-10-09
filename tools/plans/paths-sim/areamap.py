"""A top-down picture of a level: each walkable area its own colour (largest first), walls and furniture (blocked or
fence cells within the level's two cells) dark red and orange, floors no mob reaches blue, V white, a grid line every
8 blocks (the regions), labels every 8."""
import sys
sys.path.insert(0, ".")
from simlib import *
from PIL import Image, ImageDraw

path, cx, cy, cz, radius = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
lo, hi, out = float(sys.argv[6]), float(sys.argv[7]), sys.argv[8]
w = World2(load(path))
nodes = {}
for x in range(cx - radius, cx + radius + 1):
    for z in range(cz - radius, cz + radius + 1):
        for y in range(int(lo) - 1, int(hi) + 2):
            p = (x, y, z)
            if w.node_type(p) == WALKABLE and lo <= w.floor_level(p) <= hi:
                nodes[p] = None
left = set(nodes)
groups = []
while left:
    s = min(left)
    g = w.flood(s, radius, (cx, cz)) & set(nodes)
    g.add(s)
    left -= g
    groups.append(g)
groups.sort(key=lambda g: -len(g))
palette = [(60, 200, 90), (230, 200, 40), (60, 170, 230), (220, 90, 200), (120, 230, 220), (250, 140, 60),
           (170, 120, 250), (140, 200, 60)]
colour = {}
for i, g in enumerate(groups):
    for p in g:
        colour.setdefault((p[0], p[2]), palette[i] if i < len(palette) else (110, 140, 110))
S = 12
size = 2 * radius + 1
im = Image.new("RGB", (size * S + 40, size * S + 40), (18, 18, 24))
d = ImageDraw.Draw(im)
for gx in range(size):
    for gz in range(size):
        x, z = cx - radius + gx, cz - radius + gz
        c = colour.get((x, z))
        if c is None:
            ks = [w.kind((x, y, z))[0] for y in range(int(lo) + 1, int(hi) + 1)]
            if "blocked" in ks:
                c = (120, 40, 40)
            elif "thin" in ks:
                c = (170, 100, 40)
            elif "floor" in ks:
                c = (40, 60, 140)
            else:
                c = None
        if c:
            d.rectangle([40 + gx * S, 40 + gz * S, 40 + gx * S + S - 2, 40 + gz * S + S - 2], fill=c)
        if x % 8 == 0:
            d.line([40 + gx * S - 1, 40, 40 + gx * S - 1, 40 + size * S], fill=(70, 70, 90))
        if z % 8 == 0:
            d.line([40, 40 + gz * S - 1, 40 + size * S, 40 + gz * S - 1], fill=(70, 70, 90))
    if (cx - radius + gx) % 8 == 0:
        d.text((40 + gx * S, 5), str(cx - radius + gx), fill=(200, 200, 200))
for gz in range(size):
    if (cz - radius + gz) % 8 == 0:
        d.text((2, 40 + gz * S), str(cz - radius + gz), fill=(200, 200, 200))
vx, vz = radius, radius
d.ellipse([40 + vx * S + 2, 40 + vz * S + 2, 40 + vx * S + S - 4, 40 + vz * S + S - 4], fill=(255, 255, 255))
im.save(out)
for i, g in enumerate(groups[:8]):
    xs = [p[0] for p in g]; zs = [p[2] for p in g]
    print(i, palette[i], len(g), f"x {min(xs)}..{max(xs)} z {min(zs)}..{max(zs)}")
