"""Spots for the husk's sight check: V at P facing a heading, the husk summoned N blocks ahead at H (as the overlay's
ahead() places it), H reachable and with a path to P, V's eye hidden from the husk's eye by the city's triangles, and a
place within a few blocks of P from which the husk would see V."""
import sys, math
sys.path.insert(0, ".")
from simlib import *
from raylib import *
binf, obj = sys.argv[1], sys.argv[2]
feet = [float(a) for a in sys.argv[3:6]]
radius = int(sys.argv[6])
w = World2(load(binf))
T = load_tris(obj, feet)
R = Rays(T)
vx, vy, vz = (math.floor(a) for a in feet)
s = tuple(int(a) for a in sys.argv[7:10]) if len(sys.argv) > 9 else w.start(vx, vz, vy)
reach = w.flood(s, 40, (vx, vz))
cols = {}
for p in reach:
    cols.setdefault((p[0], p[2]), []).append(p)
def node_at(x, z, near):
    c = cols.get((math.floor(x), math.floor(z)))
    if not c:
        return None
    return min(c, key=lambda p: abs(w.floor_level(p) - near))
found = []
for p in sorted(reach):
    if abs(p[0] - s[0]) > radius or abs(p[2] - s[2]) > radius:
        continue
    fp = w.floor_level(p)
    eye_v = (p[0] + 0.5, fp + 1.62, p[2] + 0.5)
    for deg in range(0, 360, 15):
        # heading as the overlay shows it: 0 north (-z), 90 east (+x)
        dx, dz = math.sin(math.radians(deg)), -math.cos(math.radians(deg))
        for n in (7,):
            hx, hz = p[0] + 0.5 + dx * n, p[2] + 0.5 + dz * n
            h = node_at(hx, hz, fp)
            if h is None or abs(w.floor_level(h) - fp) > 1.0:
                continue
            fh = w.floor_level(h)
            eye_h = (hx, fh + 1.74, hz)
            if not R.hits(eye_h, eye_v):
                continue
            # a place within 4 blocks of P, reachable, from which the husk's eye sees V's
            seen_from = None
            for q in reach:
                if abs(q[0] - p[0]) + abs(q[2] - p[2]) <= 4:
                    fq = w.floor_level(q)
                    if not R.hits(eye_h, (q[0] + 0.5, fq + 1.62, q[2] + 0.5)):
                        seen_from = q
                        break
            if seen_from:
                found.append((p, deg, n, h, seen_from))
print(len(found), "spots")
for f in found[:40]:
    print(f)
