import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("raised.bin")
for middle in (True, False):
    w = World(cells, middle)
    s = w.start(1280, 2007, 6)
    reached = w.flood(s, 30, (1280, 2007))
    print("middle" if middle else "whole")
    for y in range(9, 1, -1):
        row = []
        for x in range(1278, 1292):
            k, t = w.kind((x, y, 2007))
            c = {"none": ".", "floor": "f", "blocked": "B"}[k] if (x, y, 2007) in cells else " "
            if (x, y, 2007) in reached:
                c = c.upper() if c != " " else "*"
                c = "@" if c in "*" else ("F" if c == "F" else "#")
            row.append(c)
        print(f"y{y:2d} " + "".join(row))
    print("     " + "".join(str(x % 10) for x in range(1278, 1292)))
k = 1286
for y in range(9, 1, -1):
    b = cells.get((k, y, 2007))
    if b:
        tops = column_tops(b)
        print(y, ["".join("%d" % int(t * 8) if t else "." for t in row) for row in tops])
