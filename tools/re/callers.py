# Every direct call (E8 rel32) and jmp (E9 rel32) in .text to the given VAs, with the function holding it.
import re
import struct
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from exe import data, function_of, image_base, section

targets = {int(a, 16) for a in sys.argv[1:]}
name, va, vsize, rawptr, rawsize = section(".text")
body = data[rawptr:rawptr + rawsize]
for m in re.finditer(rb"[\xE8\xE9]", body):
    p = m.start()
    if p + 5 > len(body):
        break
    rel = struct.unpack_from("<i", body, p + 1)[0]
    at = image_base + va + p
    t = at + 5 + rel
    if t in targets:
        f = function_of(at)
        print(f"{t:#x} <- {'call' if body[p] == 0xE8 else 'jmp '} at {at:#x} in {f[0]:#x}..{f[1]:#x}" if f else f"{t:#x} <- at {at:#x}")
