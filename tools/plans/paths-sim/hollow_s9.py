exec(open("fences.py").read().split("from collections import Counter")[0])
old = World(cells, True)
new = World2()
r_old = old.flood(old.start(1280, 2007, 6), 30, (1280, 2007))
r_new = new.flood(new.start(1280, 2007, 6), 30, (1280, 2007))
for name, w, r in (("1.7.0", old, r_old), ("1.7.1", new, r_new)):
    low = sorted(p for p in r if w.floor_level(p) < 5.5 and 1270 <= p[0] <= 1290 and 1985 <= p[2] <= 2010)
    hollow = [p for p in low if 1278 <= p[0] <= 1283 and 1993 <= p[2] <= 2001]
    print(name, "reachable", len(r), "below 5.5 near the road:", len(low), "in the hollow:", len(hollow))
    print("  ", [(p, w.floor_level(p)) for p in hollow[:12]])
    print("  ", [(p, w.floor_level(p)) for p in low[:12]])
