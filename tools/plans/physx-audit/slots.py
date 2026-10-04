"""Spike 5's Part 3 audit: which functions sit in the game's PhysX vtables at the slots Part 3 calls or hooks. For each
slot it disassembles the function (to its first ret, following a leading jmp), and lists the strings it references, the
calls it makes, the vtables it loads, and its size, so each can be matched to PhysX 3.4's source.

    python slots.py <vtable> <slot> [slot ...]      vtable: physics, scene, static, dynamic, shape, or an RVA
    python slots.py <vtable> <slot> --dis [n]       also prints the first n instructions (default 40)
"""
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

DLL = r"E:\Cyberpunk2077\bin\x64\PhysX3_x64.dll"
VTABLES = {"physics": 0x20A310, "scene": 0x20CAF0, "static": 0x20B980, "dynamic": 0x20B2E8, "shape": 0x20E9A8,
           "aggregate": 0x206E68}
NAMES = {v: k for k, v in VTABLES.items()}

data = open(DLL, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
count = struct.unpack_from("<H", data, pe + 6)[0]
optional_size = struct.unpack_from("<H", data, pe + 20)[0]
image_base = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
sections = []
for i in range(count):
    at = pe + 24 + optional_size + 40 * i
    name = data[at:at + 8].rstrip(b"\0").decode()
    vsize, va, rawsize, raw = struct.unpack_from("<IIII", data, at + 8)
    sections.append((name, va, max(vsize, rawsize), raw, rawsize))


def offset(rva):
    for name, va, size, raw, rawsize in sections:
        if va <= rva < va + size and rva - va < rawsize:
            return raw + (rva - va)
    return None


def section_of(rva):
    for name, va, size, raw, rawsize in sections:
        if va <= rva < va + size:
            return name
    return None


def cstring(rva, limit=200):
    at = offset(rva)
    if at is None:
        return None
    end = data.find(b"\0", at, at + limit)
    if end <= at:
        return None
    text = data[at:end]
    if all(32 <= c < 127 for c in text) and len(text) >= 4:
        return text.decode()
    return None


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True


def disassemble(rva, depth=0, show=0):
    at = offset(rva)
    code = data[at:at + 8192]
    strings, calls, vtables, lines, n = [], [], [], [], 0
    for ins in md.disasm(code, image_base + rva):
        n += 1
        if n <= show:
            lines.append(f"      {ins.address - image_base:#x}: {ins.mnemonic} {ins.op_str}")
        if ins.mnemonic == "jmp" and n == 1 and ins.operands and ins.operands[0].type == X86_OP_IMM and depth < 2:
            return disassemble(ins.operands[0].imm - image_base, depth + 1, show)  # a thunk
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                target = ins.address + ins.size + op.mem.disp - image_base
                if target in NAMES:
                    vtables.append(NAMES[target])
                s = cstring(target)
                if s and section_of(target) == ".rdata":
                    strings.append(s)
        if ins.mnemonic in ("call", "jmp") and ins.operands and ins.operands[0].type == X86_OP_IMM:
            calls.append(("" if ins.mnemonic == "call" else "jmp ") + hex(ins.operands[0].imm - image_base))
        if ins.mnemonic == "ret" or ins.mnemonic == "int3" or n > 3000:
            break
    return rva, n, strings, calls, vtables, lines


def slot_target(vtable, slot):
    return struct.unpack_from("<Q", data, offset(vtable) + 8 * slot)[0] - image_base


if __name__ == "__main__":
    which = sys.argv[1]
    vtable = VTABLES.get(which) or int(which, 16)
    args = sys.argv[2:]
    show = 0
    if "--dis" in args:
        i = args.index("--dis")
        show = int(args[i + 1]) if i + 1 < len(args) else 40
        args = args[:i]
    for slot in [int(s) for s in args]:
        target = slot_target(vtable, slot)
        rva, n, strings, calls, vtables, lines = disassemble(target, show=show)
        print(f"{which} slot {slot:3}: rva {target:#x} (code at {rva:#x}), {n} instructions to the first ret, calls {calls[:12]}"
              f"{' vtables ' + str(vtables) if vtables else ''}")
        for s in dict.fromkeys(strings):
            print(f"            \"{s[:150]}\"")
        for line in lines:
            print(line)
