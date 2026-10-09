"""The sight check's candidates, graded: V at P hidden from the husk at H, also from the 8 places around P and around
H; V's eye seen from the husk once V steps to Q; the path from H to P in walking nodes; and where the ray first hits."""
import sys, math
sys.path.insert(0, ".")
from simlib import *
from raylib import *
from collections import deque
binf, obj = sys.argv[1], sys.argv[2]
feet = [float(a) for a in sys.argv[3:6]]
s = tuple(int(a) for a in sys.argv[6:9])
cands = eval(open(sys.argv[9]).read())
w = World2(load(binf))
R = Rays(load_tris(obj, feet))
reach = w.flood(s, 40, (s[0], s[2]))
cols = {}
for p in reach:
    cols.setdefault((p[0], p[2]), []).append(p)
def eye(x, z, near, h):
    c = cols.get((math.floor(x), math.floor(z)))
    if not c:
        return None
    n = min(c, key=lambda p: abs(w.floor_level(p) - near))
    return (x, w.floor_level(n) + h, z)
def steps(a, b):
    par = {a: 0}; dq = deque([a])
    while dq:
        p = dq.popleft()
        if p == b:
            return par[p]
        for q in w.neighbours(p):
            if q in reach and q not in par:
                par[q] = par[p] + 1; dq.append(q)
    return None
out = []
for (p, deg, n, h, q) in cands:
    fp = w.floor_level(p)
    dx, dz = math.sin(math.radians(deg)), -math.cos(math.radians(deg))
    hx, hz = p[0] + 0.5 + dx * n, p[2] + 0.5 + dz * n
    hidden = 0
    total = 0
    for ax in (-1, 0, 1):
        for az in (-1, 0, 1):
            for bx in (-1, 0, 1):
                for bz in (-1, 0, 1):
                    ev = eye(p[0] + 0.5 + ax, p[2] + 0.5 + az, fp, 1.62)
                    eh = eye(hx + bx, hz + bz, fp, 1.74)
                    if ev and eh:
                        total += 1
                        hidden += R.hits(eh, ev)
    st = steps(h, p)
    hit = first_hit(R, eye(hx, hz, fp, 1.74), (p[0] + 0.5, fp + 1.62, p[2] + 0.5))
    out.append((hidden / max(1, total), st, p, deg, n, h, q, None if hit is None else tuple(round(float(v), 2) for v in hit)))
out.sort(key=lambda t: (-t[0], t[1] or 999))
for o in out[:25]:
    print(o)
