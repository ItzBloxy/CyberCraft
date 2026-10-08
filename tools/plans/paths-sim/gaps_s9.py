exec(open("fences.py").read().split("class World2")[0])
def cov(p, middle=True):
    t = TOPS.get(p)
    if t is None:
        return 0
    rng = range(1, 7) if middle else range(8)
    return sum(1 for zz in rng for xx in rng if t[zz * 8 + xx] > 0)
for z in range(1993, 2005):
    row = []
    for x in range(1270, 1284):
        a, b = cov((x, 6, z), False), cov((x, 5, z), False)
        row.append(f"{a:2d}/{b:2d}")
    print(z, " ".join(row))
print("x:  ", " ".join(f"{x % 100:5d}" for x in range(1270, 1284)))
