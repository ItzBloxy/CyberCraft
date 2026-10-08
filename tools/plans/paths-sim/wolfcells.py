import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("raised_deep.bin")
w = World(cells, True)
def cols(b):
    t = column_tops(b)
    return "".join(str(int(v * 8)) if v else "." for v in t[4])  # the middle row, z 4
print("teleport candidates around the owner's block (1276, 6, 2000): x 1273..1279, z 1997..2003, y 5..7")
for y in (5, 6, 7):
    for x in range(1273, 1280):
        for z in range(1997, 2004):
            if abs(x - 1276) < 2 and abs(z - 2000) < 2:
                continue
            if w.static_type((x, y, z)) == WALKABLE:
                k, top = w.kind((x, y, z)); kb, topb = w.kind((x, y - 1, z)); ka, topa = w.kind((x, y + 1, z))
                f = w.floor_level((x, y, z))
                if f < 5.9:
                    print(f"  ({x},{y},{z}) WALKABLE floor {f:.3f}: here {k}, below {kb} {topb}, above {ka} {topa}")
print("cells at x 1279..1282, z 1997, y 2..7:")
for x in range(1279, 1283):
    print(" x", x, [(y, w.kind((x, y, 1997))[0][0], w.kind((x, y, 1997))[1]) for y in range(2, 8)])
