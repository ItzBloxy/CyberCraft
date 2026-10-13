# tools/re

Read-only helpers for reading `Cyberpunk2077.exe` (2.31) with capstone, from the user's own Python. They found the
teleport → population chain in [the NPC spawning record](../../docs/spikes/2026-10-13-npc-spawning-in-build-mode.md).
The exe's MSVC RTTI is stripped, so classes are found through the engine's own type registration instead.

| Script | Does |
|---|---|
| `exe.py` | The PE's sections, `.pdata` functions, string search and RIP-relative `lea` references (imported by the others) |
| `ann.py <va> [end]` | Disassembles a function (or a leaf, up to its `ret`), naming the strings its RIP-relative operands hit |
| `native.py <Class> [slots...]` | A native class's vtable: its type registration's `TTypedClass` vtable, whose slot 27 builds the class, then the slots asked for |
| `callers.py <va>...` | Every direct `call`/`jmp` to the given addresses |
| `writes.py <disp> [all]` | Instructions writing (or, with `all`, using) `[reg + disp]`. Many classes share offsets, so check the class |

Run them as `python -I tools/re/<script>.py ...`. The exe's path is set in `exe.py`.
