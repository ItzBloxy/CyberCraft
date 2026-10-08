import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("raised.bin")
wm, ww = World(cells, True), World(cells, False)
rm = wm.flood(wm.start(1280, 2007, 6), 30, (1280, 2007))
rw = ww.flood(ww.start(1280, 2007, 6), 30, (1280, 2007))
extra = rw - rm
print("whole only:", len(extra), "middle only:", len(rm - rw))
# whole-rule edges from a node both reach to a node only whole reaches
gates = []
for p in rm & rw:
    for q in ww.neighbours(p):
        if q in extra:
            gates.append((p, q))
print("gateways:", len(gates))
def cols(b):
    t = column_tops(b)
    return ["".join(str(int(v * 8)) if v else "." for v in row) for row in t]
for p, q in gates[:4]:
    print("from", p, "floor %.3f" % ww.floor_level(p), "to", q, "floor %.3f" % ww.floor_level(q))
    for y in range(q[1] + 1, q[1] - 2, -1):
        c = (q[0], y, q[2])
        print("   ", c, wm.kind(c), ww.kind(c), cols(cells[c]) if c in cells else "-")
