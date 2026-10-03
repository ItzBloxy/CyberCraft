# Disassembles Cyberpunk2077.exe from a virtual address: the whole function that holds it (by .pdata), or a count of
# instructions with "n=<count>". Usage: disasm.py <address> [n=<count>]. (Not dis.py: that would hide Python's own dis
# module from capstone.)
import bisect
import struct
import sys

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
    vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
    sections.append((va, vsize, rawptr, rawsize))


def rva_to_off(rva):
    for va, vsize, rawptr, rawsize in sections:
        if va <= rva < va + max(vsize, rawsize):
            return rawptr + (rva - va)
    raise ValueError(hex(rva))


pdata_rva, pdata_size = struct.unpack_from("<II", data, directories + 3 * 8)
pdata_off = rva_to_off(pdata_rva)
functions = sorted(struct.unpack_from("<III", data, pdata_off + i * 12) for i in range(pdata_size // 12))
starts = [f[0] for f in functions]

address = int(sys.argv[1], 16)
rva = address - image_base
md = Cs(CS_ARCH_X86, CS_MODE_64)
if len(sys.argv) > 2 and sys.argv[2].startswith("n="):
    count = int(sys.argv[2][2:])
    body = data[rva_to_off(rva):rva_to_off(rva) + count * 15]
    for n, ins in enumerate(md.disasm(body, address)):
        if n >= count:
            break
        print(f"  {ins.address:#x}: {ins.mnemonic} {ins.op_str}")
else:
    k = bisect.bisect_right(starts, rva) - 1
    begin, end, _ = functions[k]
    print(f"function {image_base + begin:#x}..{image_base + end:#x} ({end - begin} bytes)")
    body = data[rva_to_off(begin):rva_to_off(begin) + (end - begin)]
    for ins in md.disasm(body, image_base + begin):
        print(f"  {ins.address:#x}: {ins.mnemonic} {ins.op_str}")
