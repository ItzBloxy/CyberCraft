import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("raised_deep.bin")
w = World(cells, True)
def grid(b):
    t = column_tops(b)
    return [" ".join(str(int(v * 8)) if v else "." for v in row) for row in t]
for z in (1982, 1983, 1984):
    for x in (1278, 1279, 1280, 1281):
        col = []
        for y in range(4, 9):
            k, top = w.kind((x, y, z))
            col.append(f"y{y}:{k[0]}{'' if top is None else round(top*8)}" + ("" if (x, y, z) in cells else "-"))
        print(f"x {x} z {z}: " + "  ".join(col))
print()
for (x, y, z) in ((1279, 7, 1982), (1280, 6, 1983), (1280, 6, 1984), (1281, 6, 1984)):
    b = cells.get((x, y, z))
    print((x, y, z), "column tops (rows z0..z7, cols x0..x7; layer+1, '.' empty):")
    if b:
        for r in grid(b): print("   ", r)
    else:
        print("    no shape")
