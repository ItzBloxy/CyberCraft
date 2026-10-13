# Cyberpunk2077.exe's sections, RVA mapping, .pdata functions and RIP-relative reference search. Read-only.
import bisect
import re
import struct

from capstone import CS_ARCH_X86, CS_MODE_64, Cs

EXE = r"E:\Cyberpunk2077\bin\x64\Cyberpunk2077.exe"
data = open(EXE, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
sections_count = struct.unpack_from("<H", data, pe + 6)[0]
optional_size = struct.unpack_from("<H", data, pe + 20)[0]
optional = pe + 24
image_base = struct.unpack_from("<Q", data, optional + 24)[0]
directories = optional + 112
sections = []
for i in range(sections_count):
    off = optional + optional_size + i * 40
    name = data[off:off + 8].rstrip(b"\0").decode()
    vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
    sections.append((name, va, vsize, rawptr, rawsize))


def section(name):
    for s in sections:
        if s[0] == name:
            return s
    raise KeyError(name)


def rva_to_off(rva):
    for name, va, vsize, rawptr, rawsize in sections:
        if va <= rva < va + min(max(vsize, rawsize), rawsize if rawsize else vsize):
            return rawptr + (rva - va)
    raise ValueError(hex(rva))


def off_to_va(off):
    for name, va, vsize, rawptr, rawsize in sections:
        if rawptr <= off < rawptr + rawsize:
            return image_base + va + (off - rawptr)
    raise ValueError(hex(off))


def va_to_off(va):
    return rva_to_off(va - image_base)


pdata_rva, pdata_size = struct.unpack_from("<II", data, directories + 3 * 8)
pdata_off = rva_to_off(pdata_rva)
functions = sorted(struct.unpack_from("<III", data, pdata_off + i * 12) for i in range(pdata_size // 12))
starts = [f[0] for f in functions]


def function_of(va):
    rva = va - image_base
    k = bisect.bisect_right(starts, rva) - 1
    begin, end, _ = functions[k]
    if begin <= rva < end:
        return image_base + begin, image_base + end
    return None


def strings(text):
    """Every VA where the NUL-terminated ASCII string text starts (preceded by NUL)."""
    needle = b"\0" + text.encode() + b"\0"
    out = []
    i = data.find(needle)
    while i >= 0:
        out.append(off_to_va(i + 1))
        i = data.find(needle, i + 1)
    return out


_text = section(".text")
_rip = re.compile(rb"[\x48\x4C]\x8D[\x05\x0D\x15\x1D\x25\x2D\x35\x3D]", re.S)


def refs(target_vas):
    """VAs of `lea r64, [rip+disp]` instructions in .text whose target is one of target_vas."""
    targets = set(target_vas)
    name, va, vsize, rawptr, rawsize = _text
    body = data[rawptr:rawptr + rawsize]
    out = []
    for m in _rip.finditer(body):
        p = m.start()
        disp = struct.unpack_from("<i", body, p + 3)[0]
        ins_va = image_base + va + p
        if ins_va + 7 + disp in targets:
            out.append(ins_va)
    return out


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = False


def disasm(va, count=None, end=None):
    off = va_to_off(va)
    size = (end - va) if end else count * 15
    out = []
    for ins in md.disasm(data[off:off + size], va):
        out.append(ins)
        if count and len(out) >= count:
            break
    return out


def show(va, count=None, end=None):
    for ins in disasm(va, count, end):
        print(f"  {ins.address:#x}: {ins.mnemonic} {ins.op_str}")
