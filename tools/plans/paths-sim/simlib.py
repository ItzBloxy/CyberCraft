"""A library for spike 9's Part 2: analyze.py without its main, and World2, 1.7.1's cells (fences.py's), with
World2.voxel for a point's voxel. analyze.py's own description follows.

Spike 9's deep review: the spec's cell rule and 26.3's walk rules, simulated on a dump's real voxels.

Reads voxdump's cells (SkyCraft ColBlocks), classifies each cell by its middle 6x6 columns (or, for comparison, all
64), applies getPathTypeStatic's below rule, getFloorLevel with floorAt, findAcceptedNode's jump limit and
tryJumpOn / tryFindFirstGroundNodeBelow for a 2-tall, under-1-wide mob, floods what's reachable from the stand-in V,
and estimates LandRandomPos's stroll odds. Writes a top-down picture.
"""
import random
import struct
import sys
from collections import deque

from PIL import Image

OPEN, WALKABLE, BLOCKED = "OPEN", "WALKABLE", "BLOCKED"


def load(path):
    data = open(path, "rb").read()
    cells = {}
    for off in range(0, len(data), 80):
        x, y, z, _ = struct.unpack_from("<iiiI", data, off)
        cells[(x, y, z)] = struct.unpack_from("<8Q", data, off + 16)
    return cells


def column_tops(bits):
    """tops[cz][cx]: the top (fraction) of each of the 64 columns, or None."""
    tops = [[None] * 8 for _ in range(8)]
    for layer in range(8):
        word = bits[layer]
        while word:
            low = word & -word
            index = low.bit_length() - 1
            word ^= low
            tops[index >> 3][index & 7] = (layer + 1) / 8
    return tops


def classify(bits, middle=True):
    tops = column_tops(bits)
    span = range(1, 7) if middle else range(8)
    best = None
    for cz in span:
        for cx in span:
            t = tops[cz][cx]
            if t is not None and (best is None or t > best):
                best = t
    if best is None:
        return ("none", None)
    return ("floor", best) if best <= 0.5 else ("blocked", best)


class World:
    def __init__(self, cells, middle=True, height=2, max_fall=3):
        self.kinds = {p: classify(b, middle) for p, b in cells.items()}
        self.height = height
        self.max_fall = max_fall

    def kind(self, p):
        return self.kinds.get(p, ("none", None))

    def from_state(self, p):
        k = self.kind(p)[0]
        return WALKABLE if k == "floor" else BLOCKED if k == "blocked" else OPEN

    def static_type(self, p):
        t = self.from_state(p)
        if t != OPEN:
            return t
        below = self.from_state((p[0], p[1] - 1, p[2]))
        return OPEN if below in (OPEN, WALKABLE) else WALKABLE

    def node_type(self, p):
        x, y, z = p
        types = [self.static_type((x, y + i, z)) for i in range(self.height)]
        if BLOCKED in types:
            return BLOCKED
        return self.static_type(p) if self.static_type(p) == OPEN else WALKABLE if WALKABLE in types else OPEN

    def floor_level(self, p):
        x, y, z = p
        k, top = self.kind(p)
        if k == "floor":
            return y + top
        kb, topb = self.kind((x, y - 1, z))
        if kb in ("floor", "blocked"):
            return y - 1 + topb
        return y - 1.0  # vanilla: the empty cell below's bottom

    def accepted(self, p, remaining, node_floor):
        x, y, z = p
        floor = self.floor_level(p)
        if floor - node_floor > 1.125:
            return None
        t = self.node_type(p)
        if t == WALKABLE:
            return p
        if t == BLOCKED:
            if remaining > 0:
                return self.accepted((x, y + 1, z), remaining - 1, node_floor)
            return None
        for i in range(y - 1, y - self.max_fall - 2, -1):
            if y - i > self.max_fall:
                return None
            ti = self.node_type((x, i, z))
            if ti != OPEN:
                return (x, i, z) if ti == WALKABLE else None
        return None

    def neighbours(self, p):
        x, y, z = p
        floor = self.floor_level(p)
        remaining = 1 if self.node_type((x, y + 1, z)) != BLOCKED else 0
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            q = self.accepted((x + dx, y, z + dz), remaining, floor)
            if q is not None:
                yield q

    def start(self, x, z, y_near):
        for dy in range(0, 4):
            for y in (y_near - dy, y_near + dy):
                if self.node_type((x, y, z)) == WALKABLE:
                    return (x, y, z)
        return None

    def flood(self, start, radius, centre):
        seen = {start}
        todo = deque([start])
        while todo:
            p = todo.popleft()
            for q in self.neighbours(p):
                if q not in seen and abs(q[0] - centre[0]) <= radius and abs(q[2] - centre[1]) <= radius:
                    seen.add(q)
                    todo.append(q)
        return seen

    def stable(self, p):
        x, y, z = p
        return self.kind(p)[0] == "floor" or self.kind((x, y - 1, z))[0] == "blocked"

    def solid(self, p):
        return self.kind(p)[0] == "blocked"

    def stroll_odds(self, nodes, trials, rng):
        nodes = list(nodes)
        hits = 0
        for _ in range(trials):
            x, y, z = rng.choice(nodes)
            ok = False
            for _ in range(10):
                # LandRandomPos: the raw candidate must be stable (isNotStable), then it's lifted out of solid
                # (movePosUpOutOfSolid), then it mustn't have a malus (hasMalus).
                c = [x + rng.randint(-10, 10), y + rng.randint(-7, 7), z + rng.randint(-10, 10)]
                if not self.stable(tuple(c)):
                    continue
                while self.solid(tuple(c)):
                    c[1] += 1
                c = tuple(c)
                if self.static_type(c) != BLOCKED:
                    ok = True
                    break
            hits += ok
        return hits / trials


def picture(world, reached, centre, radius, path, y0):
    size = 2 * radius + 1
    image = Image.new("RGB", (size * 6, size * 6), (20, 20, 28))
    top = {}
    for (x, y, z) in reached:
        if (x, z) not in top or y > top[(x, z)]:
            top[(x, z)] = y
    for gx in range(size):
        for gz in range(size):
            x, z = centre[0] - radius + gx, centre[1] - radius + gz
            colour = None
            if (x, z) in top:
                f = world.floor_level((x, top[(x, z)], z)) - y0
                shade = max(0, min(255, int(128 + f * 25)))
                colour = (60, shade, 90)
            elif any(world.kind((x, y, z))[0] == "blocked" for y in range(int(y0) - 2, int(y0) + 3)):
                colour = (150, 70, 60)
            elif any(world.kind((x, y, z))[0] != "none" for y in range(int(y0) - 10, int(y0) + 10)):
                colour = (90, 90, 90)
            if colour:
                for i in range(6):
                    for j in range(6):
                        image.putpixel((gx * 6 + i, gz * 6 + j), colour)
    cx, cz = radius, radius
    for i in range(6):
        for j in range(6):
            image.putpixel((cx * 6 + i, cz * 6 + j), (255, 255, 255))
    image.save(path)




def tops_of(b):
    t = column_tops(b)
    return [int(round((t[z][x] or 0) * 8)) for z in range(8) for x in range(8)]


class World2(World):
    """1.7.1's cells: CellRule with fences (THIN), as fences.py has it."""
    def __init__(self, cells, height=2, max_fall=3):
        self.cells = cells
        self.tops = {p: tops_of(b) for p, b in cells.items()}
        self.kinds = {p: self.classify2(p) for p in cells}
        self.height = height
        self.max_fall = max_fall

    def classify2(self, p):
        tops = self.tops.get(p)
        if tops is None:
            return ("none", None)
        below = self.tops.get((p[0], p[1] - 1, p[2]))
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

    def voxel(self, x, y, z):
        import math
        c = (math.floor(x), math.floor(y), math.floor(z))
        b = self.cells.get(c)
        if not b:
            return False
        vx, vy, vz = int((x - c[0]) * 8), int((y - c[1]) * 8), int((z - c[2]) * 8)
        return (b[vy] >> (vx + 8 * vz)) & 1 == 1
