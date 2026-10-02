"""Edit steps from two versions of a file: the (old, new) pairs that turn one into the other, applied in order, each old
block found exactly once in the text as the pairs before it left it. Each pair is one change with as little context
as keeps it unique: a line above an insertion, more lines until it's unique."""
import difflib
import re


def readable(block):
    """Whether a reader can find the block: it has a line with some words in it, not only braces or blank lines."""
    return any(len(re.findall(r"\w", line)) >= 6 for line in block.splitlines())


def make_pairs(before, after, gap=3):
    a = before.splitlines(keepends=True)
    b = after.splitlines(keepends=True)
    ops = [op for op in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if op[0] != "equal"]
    # Changes closer than `gap` unchanged lines make one pair.
    hunks = []
    for kind, a0, a1, b0, b1 in ops:
        if hunks and a0 - hunks[-1][1] <= gap:
            hunks[-1][1], hunks[-1][3] = a1, b1
        else:
            hunks.append([a0, a1, b0, b1, kind == "insert"])
    current = before
    pairs = []
    floor = 0  # lines before this were changed by earlier pairs, or are their context
    for index, (a0, a1, b0, b1, inserts_first) in enumerate(hunks):
        ceiling = hunks[index + 1][0] if index + 1 < len(hunks) else len(a)
        above = 1 if inserts_first else 0  # an insertion reads best after the line it follows
        below = 0
        while True:
            lo, hi = max(floor, a0 - above), min(ceiling, a1 + below)
            old = "".join(a[lo:hi])
            if readable(old) and current.count(old) == 1:
                break
            if lo == floor and hi == ceiling:
                raise SystemExit(f"no unique context for the change at line {a0 + 1}")
            # Grow above first, then below, alternately.
            if (above <= below and lo > floor) or hi == ceiling:
                above += 1
            else:
                below += 1
        # Blank lines at the edges of the context only make the block harder to read: drop them while it stays unique.
        while lo < a0 and not a[lo].strip():
            shorter = "".join(a[lo + 1:hi])
            if not readable(shorter) or current.count(shorter) != 1:
                break
            lo += 1
        while hi > a1 and not a[hi - 1].strip():
            shorter = "".join(a[lo:hi - 1])
            if not readable(shorter) or current.count(shorter) != 1:
                break
            hi -= 1
        old = "".join(a[lo:hi])
        new = "".join(a[lo:a0] + b[b0:b1] + a[a1:hi])
        pairs.append((old, new))
        current = current.replace(old, new)
        floor = hi
    if current != after:
        raise SystemExit("the pairs don't reproduce the new version")
    return pairs


if __name__ == "__main__":
    import sys
    from pathlib import Path

    for old, new in make_pairs(Path(sys.argv[1]).read_text(encoding="utf-8"), Path(sys.argv[2]).read_text(encoding="utf-8")):
        print("----- old\n" + old + "----- new\n" + new, end="")
