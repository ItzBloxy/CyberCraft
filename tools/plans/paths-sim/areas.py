"""An area's numbers for run F: its places, a stroll's odds of finding a target there, a pet's odds of landing beside
V (one tick's 10 tries, room for a 0.6 x 0.85 wolf at the floor), and the places with the most open floor ahead in
each heading (the overlay's summons go 2 to 10 blocks ahead)."""
import sys, math, random
sys.path.insert(0, ".")
from simlib import *
binf = sys.argv[1]
start = tuple(int(a) for a in sys.argv[2:5])
height = int(sys.argv[5])
w = World2(load(binf), height=height)
reach = w.flood(start, 40, (start[0], start[2]))
cols = {}
for p in reach:
    cols.setdefault((p[0], p[2]), []).append(p)
rng = random.Random(9)
print("places", len(reach), "stroll odds", round(w.stroll_odds(reach, 2000, rng), 2))
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
def teleport(o):
    ox, oy, oz = o
    for _ in range(10):
        while True:
            dx, dz = rng.randint(-3, 3), rng.randint(-3, 3)
            if not (abs(dx) < 2 and abs(dz) < 2):
                break
        p = (ox + dx, oy + rng.randint(-1, 1), oz + dz)
        if w.static_type(p) == WALKABLE:
            k, top = w.kind(p)
            floor = p[1] + top if k == "floor" else w.floor_level(p)
            if room(p[0], p[1], p[2], floor):
                return True
    return False
owners = sorted(reach)
ok = sum(teleport(o) for o in owners)
print("teleport odds over its places (one tick)", round(ok / len(owners), 2))
best = []
for p in owners:
    for deg in range(0, 360, 45):
        dx, dz = math.sin(math.radians(deg)), -math.cos(math.radians(deg))
        run = 0
        for n in range(1, 13):
            c = cols.get((math.floor(p[0] + 0.5 + dx * n), math.floor(p[2] + 0.5 + dz * n)))
            if not c or min(abs(w.floor_level(q) - w.floor_level(p)) for q in c) > 0.5:
                break
            run = n
        best.append((run, p, deg))
best.sort(key=lambda t: -t[0])
print("longest open runs ahead (blocks, place, heading 0 north 90 east):", best[:12])
