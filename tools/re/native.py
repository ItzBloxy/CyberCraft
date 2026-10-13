# A native class's vtable from its engine type registration: the registration stores the TTypedClass vtable next to
# the CName of the class's name; that vtable's slot 27 forwards to the class's constructor, which writes the class's
# own vtable. Usage: native.py <ClassName> [slots...]
import re
import struct
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ann import target
from exe import data, disasm, function_of, refs, strings, va_to_off


def type_vtables(name):
    out = []
    for r in refs(strings(name)):
        f = function_of(r)
        if not f:
            continue
        ins = disasm(r, end=f[1])
        # After the name: the first `lea rax, [rip+X]` followed by a store of rax to a RIP-relative global.
        last = None
        for i in ins:
            if i.mnemonic == "lea" and i.op_str.startswith("rax, [rip"):
                last = target(i)
            elif last and i.mnemonic == "mov" and i.op_str.startswith("qword ptr [rip") and i.op_str.endswith(", rax"):
                out.append(last)
                break
    return out


def slot(vt, k):
    return struct.unpack_from("<Q", data, va_to_off(vt + k * 8))[0]


def constructor_vtable(type_vt):
    f = slot(type_vt, 27)
    ins = disasm(f, count=4)
    # `mov rcx, rdx; jmp ctor` or a direct constructor.
    ctor = f
    for i in ins:
        if i.mnemonic == "jmp" and i.op_str.startswith("0x"):
            ctor = int(i.op_str, 16)
            break
    fn = function_of(ctor)
    body = disasm(ctor, end=fn[1]) if fn else disasm(ctor, count=200)
    # The last `lea reg, [rip+V]` stored to [this] (rbx or rdi, set from rcx at entry) is the class's own vtable.
    this = None
    found = []
    for i in body[:12]:
        m = re.match(r"(r\w+), rcx$", i.op_str)
        if i.mnemonic == "mov" and m:
            this = m.group(1)
            break
    regs = {}
    for i in body:
        if i.mnemonic == "lea" and "[rip" in i.op_str:
            regs[i.op_str.split(",")[0]] = target(i)
        m = re.match(r"qword ptr \[(\w+)\], (\w+)$", i.op_str)
        if i.mnemonic == "mov" and m and m.group(1) == this and m.group(2) in regs:
            found.append(regs[m.group(2)])
    return ctor, found


if __name__ == "__main__":
    name = sys.argv[1]
    for tv in type_vtables(name):
        ctor, vts = constructor_vtable(tv)
        print(f"{name}: type vtable {tv:#x}, constructor {ctor:#x}, vtable stores {[hex(v) for v in vts]}")
        for v in vts[-1:]:
            for k in [int(a, 0) for a in sys.argv[2:]]:
                print(f"  slot {k} ({k * 8:#x}): {slot(v, k):#x}")
