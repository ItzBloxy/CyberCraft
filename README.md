# Cyberpunk-Minecraft

Exploring a SkyCraft-style mod for Cyberpunk 2077: play Night City as a Minecraft player, with Minecraft running
hidden next to the game and the two linked over shared memory. See
[chasmlol/SkyCraft](https://github.com/chasmlol/SkyCraft) for the Skyrim original.

Nothing is playable yet. The work so far is a set of spikes that de-risk the hard parts.

| Folder | |
|---|---|
| `docs/superpowers/specs/` | Designs |
| `docs/superpowers/plans/` | Implementation plans |
| `spikes/physx-probe/` | Spike 1: can Cyberpunk's PhysX be read through NVIDIA's public headers? |
| `extern/` | RED4ext.SDK (a submodule) and the PhysX 3.4.2 public headers |
| `minecraft/cybercraft-tweaks/` | A Fabric mod beside SkyCraft: blasts leave Night City's ground as it is |

To clone with the submodule, use `git clone --recursive`, or run `git submodule update --init` after cloning.
