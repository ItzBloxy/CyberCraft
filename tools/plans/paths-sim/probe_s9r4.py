exec(open("fences.py").read().split("class World2")[0])
def col(x, z):
    out = []
    for y in range(8, 0, -1):
        k, top = classify2((x, y, z))
        t = TOPS.get((x, y, z))
        cov = None if t is None else sum(1 for zz in range(1, 7) for xx in range(1, 7) if t[zz * 8 + xx] > 0)
        if k != "none" or cov:
            out.append(f"{y}:{k[0]}{'' if top is None else round(y + top, 3)}/{cov}")
    return " ".join(out)
for z in range(1995, 2001):
    print(z, " || ".join(f"x{x}: {col(x, z)}" for x in range(1279, 1284)))
