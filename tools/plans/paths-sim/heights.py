import sys
args = sys.argv[1:]
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load(args[0]); cx, cz, r = int(args[1]), int(args[2]), int(args[3])
w = World(cells, True)
reached = w.flood(w.start(cx, cz, 6), 30, (cx, cz))
print("walkable floor height per column (x right, z down), *: reachable, digits: tenths over 6, ^ above 9.9, v below 6")
for z in range(cz - r, cz + r + 1):
    row = []
    for x in range(cx - r, cx + r + 1):
        best = None
        for y in range(3, 14):
            if w.node_type((x, y, z)) == WALKABLE:
                f = w.floor_level((x, y, z))
                if best is None or abs(f - 6.14) < abs(best[0] - 6.14):
                    best = (f, (x, y, z) in reached)
        if best is None: row.append("  . ")
        else:
            f, ok = best
            row.append(("*" if ok else " ") + ("%3d" % round((f - 6) * 10)))
    print("".join(row))
