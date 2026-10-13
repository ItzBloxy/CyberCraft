# Instructions in .text that write (or test) [reg + disp] for a given displacement, by a byte prefilter + disassembly.
import re
import struct
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from exe import data, disasm, function_of, image_base, section, md

disp = int(sys.argv[1], 16)
kind = sys.argv[2] if len(sys.argv) > 2 else "write"
name, va, vsize, rawptr, rawsize = section(".text")
body = data[rawptr:rawptr + rawsize]
needle = struct.pack("<I", disp)
seen = set()
p = body.find(needle)
while p >= 0:
    # Decode a few candidate starts before the displacement.
    for back in range(2, 9):
        s = p - back
        ins = next(md.disasm(body[s:s + 15], image_base + va + s), None)
        if not ins or ins.address in seen:
            continue
        if ins.address + ins.size < image_base + va + p + 4:
            continue
        txt = f"{ins.mnemonic} {ins.op_str}"
        if f"+ {disp:#x}]" not in txt:
            continue
        is_write = re.match(r"(mov|and|or|xor|inc|dec|add|sub|xchg|setn?e|set\w+) (byte|word|dword|qword) ptr \[", txt) is not None
        if (kind == "write" and is_write) or kind == "all":
            seen.add(ins.address)
            f = function_of(ins.address)
            print(f"{ins.address:#x}: {txt}   in {f[0]:#x}..{f[1]:#x}" if f else f"{ins.address:#x}: {txt}")
        break
    p = body.find(needle, p + 1)
