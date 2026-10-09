"""Pets' landings for V jumping from the room spot: candidate targets on V's storey (corridor) and the one below, 15 to
30 blocks away, each with one tick's odds of a landing with room (1,000 trials), as the teleport tries it."""
import sys, math, random
sys.path.insert(0, ".")
from simlib import *
w = World2(load("h10.bin"), height=1)
w2 = World2(load("h10.bin"), height=2)
reach = w2.flood((-1384, 123, -1272), 45, (-1384, -1272))
rng = random.Random(3)
def room(x, y, z, floor, width=0.6, h=0.85):
    hw = width / 2
    for i in range(5):
        for j in range(5):
            for k in range(8):
                px = x + 0.5 - hw + 1e-3 + (width - 2e-3) * i / 4
                pz = z + 0.5 - hw + 1e-3 + (width - 2e-3) * j / 4
                py = floor + 1e-3 + (h - 2e-3) * k / 7
                if w.voxel(px, py, pz):
                    return False
    return True
def odds(o, n=1000):
    ok = 0
    lands = []
    for _ in range(n):
        for _ in range(10):
            while True:
                dx, dz = rng.randint(-3, 3), rng.randint(-3, 3)
                if not (abs(dx) < 2 and abs(dz) < 2):
                    break
            p = (o[0] + dx, o[1] + rng.randint(-1, 1), o[2] + dz)
            if w.static_type(p) == WALKABLE:
                k, top = w.kind(p)
                floor = p[1] + top if k == "floor" else w.floor_level(p)
                if room(p[0], p[1], p[2], floor):
                    ok += 1
                    lands.append(round(floor, 3))
                    break
    return ok / n, sorted(set(lands))
start = (-1385.5, -1271.5)
for level in (123, 119):
    cands = []
    for p in reach:
        if p[1] == level and abs(w2.floor_level(p) - (level + 0.125)) < 0.01:
            d = math.hypot(p[0] + 0.5 - start[0], p[2] + 0.5 - start[1])
            if 18 <= d <= 26:
                cands.append((d, p))
    cands.sort()
    best = []
    for d, p in cands[::max(1, len(cands) // 40)]:
        o, lands = odds(p, 300)
        best.append((o, round(d, 1), p, lands))
    best.sort(key=lambda t: -t[0])
    print("storey", level, len(cands), "candidates; best:")
    for b in best[:6]:
        print("  ", b)
