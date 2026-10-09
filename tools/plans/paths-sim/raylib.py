"""A dump's static triangles at a placement (Minecraft axes, around V's feet), and segment tests against them, as
SkyClip.cast casts: does the segment p-q cross the city?"""
import numpy as np

def load_tris(obj, feet):
    verts, tris, keep = [], [], False
    for line in open(obj):
        if line.startswith("o "):
            keep = line[2:].strip().startswith("s0_static_")
        elif line.startswith("v "):
            verts.append([float(v) for v in line.split()[1:4]])
        elif line.startswith("f ") and keep:
            tris.append([int(t.split("/")[0]) - 1 for t in line.split()[1:4]])
    V = np.array(verts) + np.array(feet)
    return V[np.array(tris)]

class Rays:
    def __init__(self, T):
        self.A, self.B, self.C = T[:, 0], T[:, 1], T[:, 2]
        self.lo = T.min(axis=1)
        self.hi = T.max(axis=1)

    def hits(self, p, q):
        p, q = np.asarray(p, float), np.asarray(q, float)
        lo, hi = np.minimum(p, q) - 0.01, np.maximum(p, q) + 0.01
        m = np.all(self.hi >= lo, axis=1) & np.all(self.lo <= hi, axis=1)
        if not m.any():
            return False
        A, B, C = self.A[m], self.B[m], self.C[m]
        d = q - p
        e1, e2 = B - A, C - A
        h = np.cross(d, e2)
        a = np.einsum("ij,ij->i", e1, h)
        ok = np.abs(a) > 1e-12
        f = np.where(ok, 1.0 / np.where(ok, a, 1), 0)
        s = p - A
        u = f * np.einsum("ij,ij->i", s, h)
        qv = np.cross(s, e1)
        v = f * (qv @ d)
        t = f * np.einsum("ij,ij->i", e2, qv)
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t >= 0) & (t <= 1)
        return bool(hit.any())

def first_hit(R, p, q):
    p, q = np.asarray(p, float), np.asarray(q, float)
    lo, hi = np.minimum(p, q) - 0.01, np.maximum(p, q) + 0.01
    m = np.all(R.hi >= lo, axis=1) & np.all(R.lo <= hi, axis=1)
    if not m.any():
        return None
    A, B, C = R.A[m], R.B[m], R.C[m]
    d = q - p
    e1, e2 = B - A, C - A
    h = np.cross(d, e2)
    a = np.einsum("ij,ij->i", e1, h)
    ok = np.abs(a) > 1e-12
    f = np.where(ok, 1.0 / np.where(ok, a, 1), 0)
    s = p - A
    u = f * np.einsum("ij,ij->i", s, h)
    qv = np.cross(s, e1)
    v = f * (qv @ d)
    t = f * np.einsum("ij,ij->i", e2, qv)
    hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t >= 0) & (t <= 1)
    if not hit.any():
        return None
    tm = t[hit].min()
    return p + d * tm
