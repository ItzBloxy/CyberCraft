import sys, random
args = sys.argv[1:]
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load(args[0]); cx, cz = int(args[1]), int(args[2])
w = World(cells, True)
reached = w.flood(w.start(cx, cz, 6), 30, (cx, cz))
def solid_voxel(x, y, z):
    # the voxel containing the point, from the cell's bits
    import math
    c = (math.floor(x), math.floor(y), math.floor(z))
    b = cells.get(c)
    if not b: return False
    vx, vy, vz = int((x - c[0]) * 8), int((y - c[1]) * 8), int((z - c[2]) * 8)
    return (b[vy] >> (vx + 8 * vz)) & 1 == 1
def room(x, y, z, floor, width=0.6, height=0.85):
    # sample the wolf's box at the landing: x+0.5, floor, z+0.5
    h = width / 2
    for i in range(5):
        for j in range(5):
            for k in range(8):
                px = x + 0.5 - h + 1e-3 + (width - 2e-3) * i / 4
                pz = z + 0.5 - h + 1e-3 + (width - 2e-3) * j / 4
                py = floor + 1e-3 + (height - 2e-3) * k / 7
                if solid_voxel(px, py, pz): return False
    return True
rng = random.Random(9)
owners = rng.sample(sorted(reached), 400)
ok_type = ok_room = 0
for (ox, oy, oz) in owners:
    t_hit = r_hit = False
    for _ in range(10):
        while True:
            dx, dz = rng.randint(-3, 3), rng.randint(-3, 3)
            if not (abs(dx) < 2 and abs(dz) < 2): break
        p = (ox + dx, oy + rng.randint(-1, 1), oz + dz)
        if w.static_type(p) == WALKABLE:
            t_hit = True
            k, top = w.kind(p)
            floor = p[1] + top if k == "floor" else w.floor_level(p)
            if room(p[0], p[1], p[2], floor): r_hit = True; break
    ok_type += t_hit; ok_room += r_hit
print(f"owners {len(owners)}: a WALKABLE try {ok_type/len(owners):.2f}, and with room at the floor {ok_room/len(owners):.2f}")
