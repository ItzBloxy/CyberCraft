# Disassembles a VA range or a whole function, annotating RIP-relative operands with the string or address they hit.
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from exe import data, disasm, function_of, va_to_off


def target(ins):
    m = re.search(r"rip ([+-]) (0x[0-9a-f]+)", ins.op_str)
    if not m:
        return None
    return ins.address + ins.size + int(m.group(2), 16) * (1 if m.group(1) == "+" else -1)


def note(t):
    try:
        o = va_to_off(t)
    except Exception:
        return f"  ; -> {t:#x}"
    s = data[o:o + 64]
    if b"\0" in s:
        n = s.index(b"\0")
        if n > 2 and all(32 <= c < 127 for c in s[:n]):
            return f'  ; "{s[:n].decode()}"'
    return f"  ; -> {t:#x}"


def annotate(a, b=None):
    if b is None:
        f = function_of(a)
        if not f:
            # A leaf function has no unwind data: read up to its first ret.
            print(f"{a:#x} (no unwind data: up to the first ret)")
            for ins in disasm(a, count=64):
                t = target(ins)
                print(f"  {ins.address:#x}: {ins.mnemonic} {ins.op_str}{note(t) if t else ''}")
                if ins.mnemonic == "ret":
                    break
            return
        a, b = f
        print(f"function {a:#x}..{b:#x}")
    for ins in disasm(a, end=b):
        t = target(ins)
        print(f"  {ins.address:#x}: {ins.mnemonic} {ins.op_str}{note(t) if t else ''}")


if __name__ == "__main__":
    a = int(sys.argv[1], 16)
    b = int(sys.argv[2], 16) if len(sys.argv) > 2 else None
    annotate(a, b)
