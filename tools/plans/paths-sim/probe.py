import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("today.bin")
w = World(cells, True)
s = w.start(1480, 2007, 6)
print("start", s, w.floor_level(s), "neighbours", list(w.neighbours(s)))
def cols(b):
    t = column_tops(b)
    return ["".join(str(int(v * 8)) if v else "." for v in row) for row in t]
for x in (1480, 1481):
    for y in range(9, 4, -1):
        c = (x, y, 2007)
        print(c, w.kind(c), w.static_type(c), w.node_type(c), cols(cells[c]) if c in cells else "-")
