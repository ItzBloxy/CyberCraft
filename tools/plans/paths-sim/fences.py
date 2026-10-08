import sys
sys.argv = ["x"]
exec(open("analyze.py").read().split("def main():")[0])
cells = load("raised_deep.bin")

def tops_of(b):
    t = column_tops(b)
    return [int(round((t[z][x] or 0) * 8)) for z in range(8) for x in range(8)]

TOPS = {p: tops_of(b) for p, b in cells.items()}

def classify2(p):
    tops = TOPS.get(p)
    if tops is None:
        return ("none", None)
    below = TOPS.get((p[0], p[1] - 1, p[2]))
    top = covered = 0
    for z in range(1, 7):
        for x in range(1, 7):
            i = z * 8 + x
            top = max(top, tops[i])
            if tops[i] > 0 or (below is not None and below[i] > 4):
                covered += 1
    if top == 0:
        return ("none", None)
    if covered < 18:
        return ("thin", top / 8)
    return ("floor", top / 8) if top <= 4 else ("blocked", top / 8)

class World2(World):
    def __init__(self):
        self.kinds = {p: classify2(p) for p in cells}
        self.height = 2
        self.max_fall = 3
    def from_state(self, p):
        k = self.kind(p)[0]
        return WALKABLE if k == "floor" else BLOCKED if k in ("blocked", "thin") else OPEN
    def floor_level(self, p):
        x, y, z = p
        k, top = self.kind(p)
        if k == "floor":
            return y + top
        kb, topb = self.kind((x, y - 1, z))
        if kb in ("floor", "blocked", "thin"):
            return y - 1 + topb + (0.5 if kb == "thin" else 0)
        return y - 1.0
    def stable(self, p):
        x, y, z = p
        return self.kind(p)[0] == "floor" or self.kind((x, y - 1, z))[0] == "blocked"
    def solid(self, p):
        return self.kind(p)[0] in ("blocked", "thin")

from collections import Counter
old = World(cells, True)
new = World2()
print("kinds before:", Counter(k for k, _ in old.kinds.values()), "after:", Counter(k for k, _ in new.kinds.values()))
r_old = old.flood(old.start(1280, 2007, 6), 30, (1280, 2007))
r_new = new.flood(new.start(1280, 2007, 6), 30, (1280, 2007))
fl = lambda w, r: sorted(w.floor_level(p) for p in r)
print("reachable before:", len(r_old), "floors", fl(old, r_old)[0], "to", fl(old, r_old)[-1])
print("reachable after: ", len(r_new), "floors", fl(new, r_new)[0], "to", fl(new, r_new)[-1])
lost = r_old - r_new
print("lost:", len(lost), "of which on the barrier's top (floor >= 7.25):", sum(1 for p in lost if old.floor_level(p) >= 7.25))
low_lost = [p for p in lost if old.floor_level(p) < 7.25]
print("lost below 7.25 (road places now unreachable):", len(low_lost), sorted(low_lost)[:15])
print("barrier top nodes after:", sum(1 for p in r_new if new.floor_level(p) >= 7.25))
import random
print("stroll odds after:", round(new.stroll_odds(r_new, 3000, random.Random(9)), 2))
high = sorted(p for p in r_new if new.floor_level(p) >= 7.25)
for p in high:
    x, y, z = p
    print(p, "floor", new.floor_level(p), "here", new.kind(p), "below", new.kind((x, y - 1, z)), "below2", new.kind((x, y - 2, z)))
