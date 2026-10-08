"""Rays against the raised-level dump's triangles (placed at the stand-in V's feet, as mclink places them), as
SkyClip.cast casts: does a segment from V's eye to a mob's eye cross the city?"""
import numpy as np
FEET = np.array([1280.5, 6.14, 2007.5])
verts, tris, keep = [], [], False
for line in open("E:/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps/2026-10-01_14-37-07/collision.obj"):
    if line.startswith("o "):
        keep = line[2:].strip().startswith("s0_static_")
    elif line.startswith("v "):
        verts.append([float(v) for v in line.split()[1:4]])
    elif line.startswith("f ") and keep:
        tris.append([int(t.split("/")[0]) - 1 for t in line.split()[1:4]])
V = np.array(verts) + FEET
T = V[np.array(tris)]
A, B, C = T[:, 0], T[:, 1], T[:, 2]
print(len(T), "triangles")

def hits(p, q):
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
    m = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t >= 0) & (t <= 1)
    return m.any()

import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
w = World(load("raised_deep.bin"), True)
def floor(x, z, near):
    best = None
    for y in range(-26, 12):
        if w.node_type((x, y, z)) == WALKABLE:
            f = w.floor_level((x, y, z))
            if best is None or abs(f - near) < abs(best - near):
                best = f
    return best
for z in (2003, 2007, 2011):
    for vx in (1276, 1279, 1282):
        vf = floor(vx, z, 6.2)
        eye_v = np.array([vx + 0.5, vf + 1.62, z + 0.5])
        row = []
        for sx in range(1288, 1302, 2):
            sf = floor(sx, z, -2)
            eye_s = np.array([sx + 0.5, sf + 1.74, z + 0.5])
            dist = np.linalg.norm(eye_s - eye_v)
            row.append(f"x{sx}:{'hid' if hits(eye_s, eye_v) else 'SEE'}({dist:.0f})")
        print(f"V at x {vx} z {z} floor {vf}: " + " ".join(row))
