"""SkyCraft's lift after a teleport, on a dump's triangles: TriCollider.floor with walkableOnly false, the highest
triangle height at the 17 footprint samples (centre, 8 at 0.15, 8 at 0.075) no higher than the feet + 2.5; a
triangle counts if |n.y| >= 0.05 and the sample is inside its xz projection. Prints the lift for each feet given."""
import sys, math
import numpy as np
sys.path.insert(0, ".")
from raylib import load_tris
obj = sys.argv[1]
place = [float(a) for a in sys.argv[2:5]]
T = load_tris(obj, place)
A, B, C = T[:, 0], T[:, 1], T[:, 2]
n = np.cross(B - A, C - A)
n = n / np.linalg.norm(n, axis=1, keepdims=True)
lo, hi = T.min(axis=1), T.max(axis=1)
samples = [(0.0, 0.0)] + [(math.cos(i * math.pi / 4) * r, math.sin(i * math.pi / 4) * r) for r in (0.15, 0.075) for i in range(8)]
def ground(x, y, z, above=2.5):
    limit = y + above
    m = (lo[:, 1] <= limit) & (np.abs(n[:, 1]) >= 0.05) & (hi[:, 0] >= x - 0.2) & (lo[:, 0] <= x + 0.2) & (hi[:, 2] >= z - 0.2) & (lo[:, 2] <= z + 0.2)
    best, which = -math.inf, None
    for k in np.nonzero(m)[0]:
        a, b, c, nn = A[k], B[k], C[k], n[k]
        for sx, sz in samples:
            px, pz = x + sx, z + sz
            def edge(x0, z0, x1, z1):
                return (x1 - x0) * (pz - z0) - (z1 - z0) * (px - x0)
            d = (edge(a[0], a[2], b[0], b[2]), edge(b[0], b[2], c[0], c[2]), edge(c[0], c[2], a[0], a[2]))
            if any(v < -1e-12 for v in d) and any(v > 1e-12 for v in d):
                continue
            h = a[1] - (nn[0] * (px - a[0]) + nn[2] * (pz - a[2])) / nn[1]
            if h <= limit and h > best:
                best, which = h, k
    return best, which
for spot in sys.argv[5:]:
    x, y, z = (float(v) for v in spot.split(","))
    g, k = ground(x, y, z)
    print(f"feet {x} {y} {z}: ground {g:.3f}, lift {max(0.0, g - y):.3f}; triangle y {lo[k][1]:.3f}..{hi[k][1]:.3f}, |ny| {abs(n[k][1]):.2f}" if k is not None else f"feet {spot}: none")
