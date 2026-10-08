import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("raised_deep.bin")
w = World(cells, True)
print("walkable floors by column x (east), at three z rows; '.' none")
for z in (1997, 2007, 2017):
    print("z", z)
    for x in range(1270, 1321, 2):
        fl = [round(w.floor_level((x, y, z)), 2) for y in range(-26, 12) if w.node_type((x, y, z)) == WALKABLE]
        print("  x", x, fl)
