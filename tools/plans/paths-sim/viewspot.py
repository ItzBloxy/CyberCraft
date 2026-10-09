"""Places for V to step into the husk's view: on V's level near the hiding spot, V's eye seen from the husk's (where it
was summoned) by the dump's triangles, no SkyCraft lift there (a header within 2.5 blocks above) or a quarter block
around, with the husk's walk to it in nodes. In the room first, so the husk has to come through the door."""
import sys, math, os
from collections import deque
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from simlib import *
from raylib import load_tris, Rays
import numpy as np
OBJ = "E:/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps/2026-10-09_12-22-29/collision.obj"
T = load_tris(OBJ, [-1383.343, 123.065, -1271.700])
R = Rays(T)
A, B, C = T[:, 0], T[:, 1], T[:, 2]
n = np.cross(B - A, C - A); nl = np.linalg.norm(n, axis=1, keepdims=True); nl[nl == 0] = 1; n = n / nl
lo, hi = T.min(axis=1), T.max(axis=1)
samples = [(0.0, 0.0)] + [(math.cos(i * math.pi / 4) * 0.15, math.sin(i * math.pi / 4) * 0.15) for i in range(8)] + \
    [(math.cos(math.pi / 4 + i * math.pi / 2) * 0.075, math.sin(math.pi / 4 + i * math.pi / 2) * 0.075) for i in range(4)]
def ground(x, y, z, above=2.5):
    """SkyCraft's TriCollider.floor(walkableOnly false): the highest surface at the samples up to y + above."""
    limit = y + above
    m = (lo[:, 1] <= limit) & (np.abs(n[:, 1]) >= 0.05) & (hi[:, 0] >= x - 0.2) & (lo[:, 0] <= x + 0.2) & (hi[:, 2] >= z - 0.2) & (lo[:, 2] <= z + 0.2)
    best = -math.inf
    for k in np.nonzero(m)[0]:
        a, b, c, nn = A[k], B[k], C[k], n[k]
        for sx, sz in samples:
            px, pz = x + sx, z + sz
            e = lambda x0, z0, x1, z1: (x1 - x0) * (pz - z0) - (z1 - z0) * (px - x0)
            d = (e(a[0], a[2], b[0], b[2]), e(b[0], b[2], c[0], c[2]), e(c[0], c[2], a[0], a[2]))
            if any(v < -1e-12 for v in d) and any(v > 1e-12 for v in d):
                continue
            h = a[1] - (nn[0] * (px - a[0]) + nn[2] * (pz - a[2])) / nn[1]
            if h <= limit and h > best:
                best = h
    return best
if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    w = World2(load("h10.bin"), height=2)
    reach = w.flood((-1384, 123, -1272), 45, (-1384, -1272))
    H = (-1391, 123, -1275)
    husk_eye = (-1390.56, 123.125 + 1.74, -1275.0)
    dist = {H: 0}; dq = deque([H])
    while dq:
        p = dq.popleft()
        for q in w.neighbours(p):
            if q in reach and q not in dist and abs(q[0] + 1386) <= 12 and abs(q[2] + 1272) <= 12:
                dist[q] = dist[p] + 1; dq.append(q)
    out = []
    for q in reach:
        if q[1] != 123 or abs(q[0] + 1384.5) > 6 or abs(q[2] + 1271.5) > 6 or q not in dist:
            continue
        f = w.floor_level(q)
        if not (123.0 <= f <= 123.5):
            continue
        cx, cz = q[0] + 0.5, q[2] + 0.5
        if R.hits(husk_eye, (cx, f + 1.62, cz)):
            continue
        feet = f - 0.06
        lifts = [ground(cx + dx, feet, cz + dz) - feet for dx, dz in ((0, 0), (0.25, 0), (-0.25, 0), (0, 0.25), (0, -0.25))]
        if max(lifts) > 0.1:
            continue
        seen = sum(not R.hits(husk_eye, (cx + dx, f + 1.62, cz + dz)) for dx in (-0.3, 0, 0.3) for dz in (-0.3, 0, 0.3))
        inroom = q[0] >= -1387
        out.append((not inroom, -seen, dist[q], q, round(f, 3), round(max(lifts), 3), round(math.hypot(cx + 1384.5, cz + 1271.5), 1)))
    out.sort()
    for o in out[:20]:
        print("in room" if not o[0] else "outside", "seen from", -o[1], "of 9;", "walk", o[2], "nodes;", o[3], "floor", o[4],
              "lift", o[5], "from the hiding spot", o[6])
