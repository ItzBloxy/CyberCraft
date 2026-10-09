"""Top-down voxel picture of a level: per 1/8-block column, the highest voxel from base + 0.2 to base + 2.4 (what
stands on the floor, up to head height), dark where none; reached places tinted green; V white; a grid line a block."""
import sys, math
sys.path.insert(0, ".")
from simlib import *
from PIL import Image, ImageDraw
path, fx, fy, fz, radius, out = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
base = float(sys.argv[7])
w = World2(load(path))
vx, vy, vz = math.floor(fx), math.floor(fy), math.floor(fz)
s = w.start(vx, vz, vy)
reach = w.flood(s, 40, (vx, vz))
lo_f, hi_f = float(sys.argv[8]), float(sys.argv[9])
rcols = {(p[0], p[2]) for p in reach if lo_f <= w.floor_level(p) <= hi_f}
P = 4
n = (2 * radius + 1) * 8
im = Image.new("RGB", (n * P, n * P), (0, 0, 0))
px = im.load()
y0, y1 = base + 0.2, base + 2.4
for gx in range(2 * radius + 1):
    for gz in range(2 * radius + 1):
        x, z = vx - radius + gx, vz - radius + gz
        cols = [[None] * 8 for _ in range(8)]
        floorcol = [[None] * 8 for _ in range(8)]
        for y in range(math.floor(y0) - 1, math.ceil(y1) + 1):
            b = w.cells.get((x, y, z))
            if not b:
                continue
            for layer in range(8):
                hy = y + (layer + 1) / 8
                word = b[layer]
                for i in range(64):
                    if word >> i & 1:
                        cx, cz = i & 7, i >> 3
                        if y0 <= hy <= y1:
                            if cols[cz][cx] is None or hy > cols[cz][cx]:
                                cols[cz][cx] = hy
                        elif base - 0.5 <= hy < y0:
                            floorcol[cz][cx] = hy
        for cz in range(8):
            for cx in range(8):
                h = cols[cz][cx]
                if h is not None:
                    t = (h - y0) / (y1 - y0)
                    c = (int(120 + 135 * t), int(110 - 60 * t), int(60 + 40 * (1 - t)))
                elif floorcol[cz][cx] is not None:
                    c = (55, 55, 75)
                else:
                    c = (0, 0, 0)
                if (x, z) in rcols and h is None:
                    c = (40, 120, 60)
                for i in range(P):
                    for j in range(P):
                        px[(gx * 8 + cx) * P + i, (gz * 8 + cz) * P + j] = c
d = ImageDraw.Draw(im)
for g in range(2 * radius + 2):
    col = (90, 90, 110) if (vx - radius + g) % 8 else (160, 160, 200)
    d.line([g * 8 * P, 0, g * 8 * P, n * P], fill=col if g % 1 == 0 else col)
    colz = (90, 90, 110) if (vz - radius + g) % 8 else (160, 160, 200)
    d.line([0, g * 8 * P, n * P, g * 8 * P], fill=colz)
fxp, fzp = (fx - (vx - radius)) * 8 * P, (fz - (vz - radius)) * 8 * P
d.ellipse([fxp - 6, fzp - 6, fxp + 6, fzp + 6], outline=(255, 255, 255), width=2)
im.save(out)
print("saved", out, im.size, "reached", len(reach))
