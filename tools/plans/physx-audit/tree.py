"""Follows a function's calls a few levels deep, listing each function's strings and the vtables it loads.

    python tree.py <rva> [depth]
"""
import sys
from slots import disassemble

seen = set()


def walk(rva, depth, limit, indent=""):
    if rva in seen:
        print(f"{indent}{rva:#x}: (seen)")
        return
    seen.add(rva)
    code, n, strings, calls, vtables, _ = disassemble(rva)
    print(f"{indent}{rva:#x} (code {code:#x}): {n} instructions{' vtables ' + str(vtables) if vtables else ''}")
    for s in dict.fromkeys(strings):
        print(f"{indent}    \"{s[:140]}\"")
    if depth < limit:
        for call in dict.fromkeys(calls):
            target = int(call.split()[-1], 16)
            walk(target, depth + 1, limit, indent + "  ")


walk(int(sys.argv[1], 16), 0, int(sys.argv[2]) if len(sys.argv) > 2 else 2)
