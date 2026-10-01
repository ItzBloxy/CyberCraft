# NVIDIA PhysX 3.4.2 public headers

Copied from https://github.com/NVIDIAGameWorks/PhysX-3.4 at commit 5e42a5f112351a223c19c17bb331e6c55037b8eb
("PhysX 3.4, APEX 1.4 patch release @25354359"): `PhysX_3.4/Include` and `PxShared/include` only. The licence is
BSD-3-Clause: see `LICENSE.md`, and the notice in each header.

One local change makes them match the PhysX that Cyberpunk 2077 2.31 ships (`PhysX3_x64.dll`, FileVersion string
3.4.2.0). `PhysX_3.4/Include/PxShape.h` declares `getReferenceCount()` right after `acquireReference()`: the game's
build has that virtual function there, and NVIDIA's public headers don't. Everything else is unmodified. See
`docs/spikes/2026-10-01-physx-abi-audit.md`.
