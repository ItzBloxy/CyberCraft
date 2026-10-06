"""Rewrap the paragraphs (or list items) of a Markdown file that start with given prefixes, to a width."""
import sys
import textwrap
from pathlib import Path

path, width, starts = Path(sys.argv[1]), int(sys.argv[2]), sys.argv[3:]
lines = path.read_text(encoding="utf-8").split("\n")
out, i = [], 0
while i < len(lines):
    line = lines[i]
    if any(line.startswith(s) for s in starts):
        bullet = line.startswith("- ")
        para = [line]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith("- ") and (not bullet or lines[i].startswith("  ")):
            para.append(lines[i])
            i += 1
        text = " ".join(part.strip() for part in para)
        if bullet:
            out += textwrap.wrap(text[2:], width, initial_indent="- ", subsequent_indent="  ", break_long_words=False,
                                 break_on_hyphens=False)
        else:
            out += textwrap.wrap(text, width, break_long_words=False, break_on_hyphens=False)
        continue
    out.append(line)
    i += 1
path.write_bytes("\n".join(out).encode("utf-8"))
