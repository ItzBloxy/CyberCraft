"""SkyCraft's lift along a walk: the path from V's room out of the door, along the corridor and down the stairs to the
storey below, each node's lift for V's feet at its floor (less 0.06, the skin's), and the stairs' top and bottom."""
import sys, os, math
from collections import deque
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.chdir(os.path.dirname(os.path.abspath(__file__)))
from viewspot import ground, World2, load
w = World2(load("h10.bin"), height=2)
s = (-1386, 123, -1272)
par = {s: None}; dq = deque([s]); hit = None
while dq:
    p = dq.popleft()
    if p[1] == 119 and abs(w.floor_level(p) - 119.125) < 0.01 and p[2] <= -1290:
        hit = p; break
    for q in w.neighbours(p):
        if q not in par and abs(q[0] + 1386) <= 30 and abs(q[2] + 1272) <= 30:
            par[q] = p; dq.append(q)
path = []
while hit: path.append(hit); hit = par[hit]
path.reverse()
for p in path:
    f = w.floor_level(p)
    lift = ground(p[0] + 0.5, f - 0.06, p[2] + 0.5) - (f - 0.06)
    print(p, round(f, 3), "lift", round(lift, 3), "<-- lifted" if lift > 0.3 else "")
