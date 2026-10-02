# SkyCraft's shared-memory protocol

`skycraft_protocol.h` and `LICENSE` (MIT) copied unmodified from https://github.com/chasmlol/SkyCraft at commit
`bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32` (SkyCraft 0.1.2): `protocol/skycraft_protocol.h` and `LICENSE`.

It's the layout of the mapping `Local\SkyCraft_v1`, protocol version 11, that SkyCraft's Fabric mod
(`skycraft-fabric-0.1.2.jar`) opens. BlockRender plays the game side, as SkyCraft's Skyrim plugin does: see
`docs/superpowers/specs/2026-10-02-minecraft-link-design.md`. The header's own static checks pin the layout.
