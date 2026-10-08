exec(open("fences.py").read().split("class World2")[0])
import sys
def col(x, z):
    out = []
    for y in range(8, -4, -1):
        k, top = classify2((x, y, z))
        if k != "none":
            out.append(f"{y}:{k[0]}{'' if top is None else round(y + top, 3)}")
    return " ".join(out)
for x, z in [(1282, 2015), (1283, 2015), (1284, 2015), (1285, 2015), (1286, 2015), (1284, 2011), (1285, 2011),
             (1283, 2008), (1284, 2008), (1285, 2008), (1283, 2006), (1284, 2006), (1285, 2006), (1286, 2006),
             (1287, 2006), (1288, 2006)]:
    print(x, z, "|", col(x, z))
