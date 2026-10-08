import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
w = World(load("raised_deep.bin"), True)
reached = w.flood(w.start(1280, 2007, 6), 40, (1280, 2007))
print("road floors near 6.1 (x across, z down); * reachable from V; . none")
print("        " + " ".join(f"{x % 100:>4}" for x in range(1270, 1286)))
for z in range(1975, 2041, 3):
    row = []
    for x in range(1270, 1286):
        best = None
        for y in range(3, 10):
            if w.node_type((x, y, z)) == WALKABLE:
                f = w.floor_level((x, y, z))
                if best is None or abs(f - 6.1) < abs(best[0] - 6.1):
                    best = (f, (x, y, z) in reached)
        row.append("   ." if best is None else ("*" if best[1] else " ") + f"{best[0]:3.1f}")
    print(f"z {z} " + " ".join(row))
