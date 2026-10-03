# Finds Cyberpunk2077.exe's calls through chosen USER32 import slots, and disassembles the functions that make them.
# Spike 4's Part 3 used it to read how the game takes Raw Input (the spec's "Cyberpunk's input").
#
#     python find_calls.py GetRawInputData,RegisterRawInputDevices [dump]
#
# capstone comes from the user's own Python; with uv, add "--with capstone" (a download: ask first).
import bisect
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, Cs

EXE = r"E:\Cyberpunk2077\bin\x64\Cyberpunk2077.exe"
WANTED = sys.argv[1].split(",") if len(sys.argv) > 1 else ["GetRawInputData", "RegisterRawInputDevices"]
DUMP = len(sys.argv) > 2 and sys.argv[2] == "dump"

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


def rva_to_off(rva):
    for name, va, vsize, rawptr, rawsize in sections:
        if va <= rva < va + max(vsize, rawsize):
            return rawptr + (rva - va)
    raise ValueError(hex(rva))


def cstr(off):
    return data[off:data.index(b"\0", off)].decode()


# The import directory: each USER32 import's IAT slot.
import_rva, import_size = struct.unpack_from("<II", data, directories + 8)
slots = {}
off = rva_to_off(import_rva)
while True:
    oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", data, off)
    if name_rva == 0:
        break
    dll = cstr(rva_to_off(name_rva))
    if dll.lower() == "user32.dll":
        i = 0
        while True:
            thunk = struct.unpack_from("<Q", data, rva_to_off(oft) + i * 8)[0]
            if thunk == 0:
                break
            if not thunk >> 63:
                slots[cstr(rva_to_off(thunk & 0x7FFFFFFF) + 2)] = image_base + ft + i * 8
            i += 1
    off += 20
for name in WANTED:
    print(f"{name}: IAT slot {slots[name]:#x}")

# .pdata: function bounds.
pdata_rva, pdata_size = struct.unpack_from("<II", data, directories + 3 * 8)
pdata_off = rva_to_off(pdata_rva)
functions = [struct.unpack_from("<III", data, pdata_off + i * 12) for i in range(pdata_size // 12)]
functions.sort()
starts = [f[0] for f in functions]

text = next(s for s in sections if s[0] == ".text")
_, text_va, text_vsize, text_raw, _ = text
code = data[text_raw:text_raw + text_vsize]
targets = {slots[name]: name for name in WANTED}
md = Cs(CS_ARCH_X86, CS_MODE_64)
seen = set()
# Calls and jumps through a slot (FF 15, FF 25), and loads of it into a register (48/4C 8B with a RIP-relative operand).
for opcode in (b"\xff\x15", b"\xff\x25", b"\x48\x8b\x05", b"\x48\x8b\x0d", b"\x48\x8b\x15", b"\x48\x8b\x1d", b"\x48\x8b\x35",
               b"\x48\x8b\x3d", b"\x4c\x8b\x05", b"\x4c\x8b\x0d", b"\x4c\x8b\x15", b"\x4c\x8b\x1d"):
    start = 0
    while True:
        i = code.find(opcode, start)
        if i < 0:
            break
        start = i + 1
        disp_at = i + len(opcode)
        length = len(opcode) + 4
        disp = struct.unpack_from("<i", code, disp_at)[0]
        target = image_base + text_va + i + length + disp
        if target in targets:
            rva = text_va + i
            k = bisect.bisect_right(starts, rva) - 1
            begin, end, _ = functions[k]
            print(f"{targets[target]}: {opcode.hex()} at {image_base + rva:#x}, in function {image_base + begin:#x}..{image_base + end:#x} ({end - begin} bytes)")
            if DUMP and begin not in seen:
                seen.add(begin)
                body = data[rva_to_off(begin):rva_to_off(begin) + (end - begin)]
                for ins in md.disasm(body, image_base + begin):
                    mark = "   <==" if ins.address == image_base + rva else ""
                    print(f"  {ins.address:#x}: {ins.mnemonic} {ins.op_str}{mark}")
