"""Packs a CyberCraft release from a BlockRender build and a CyberCraft Tweaks jar.

    python release/package.py <BlockRender build folder> <cybercraft-tweaks jar> <release version> [output folder]

The build folder is the one holding BlockRender.dll and BlockRender.pdb (e.g. spikes/block-render/build/RelWithDebInfo).
Writes into the output folder (default: release/out):
- CyberCraft-<version>.zip: the game's side, laid out as the game folder, for Vortex, MO2 or unzipping by hand;
- the Tweaks jar, unchanged, for Minecraft's mods folder;
- BlockRender-<plugin version>-pdb.zip: the plugin's debug symbols, for reading crash dumps.
"""
import re
import shutil
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PLUGIN = REPO / "spikes" / "block-render"
PLUGIN_DIR = "red4ext/plugins/BlockRender"
HUD_DIR = "bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud"

# The licenses of what the plugin is built with, reproduced beside the binary as they ask.
THIRD_PARTY = [
    ("SkyCraft (shared-memory protocol header)", "extern/skycraft/LICENSE"),
    ("RED4ext.SDK", "extern/RED4ext.SDK/LICENSE.md"),
    ("RED4ext.SDK's third-party code", "extern/RED4ext.SDK/THIRD_PARTY_LICENSES.md"),
    ("NVIDIA PhysX SDK 3.4 (public headers)", "extern/physx-3.4.2/LICENSE.md"),
    ("NVIDIA Streamline 2.7.2 (public headers)", "extern/streamline-2.7.2/license.txt"),
]


def plugin_version() -> str:
    source = (PLUGIN / "src" / "Plugin.cpp").read_text(encoding="utf-8")
    return re.search(r'kVersion = "([0-9.]+)"', source).group(1)


def third_party_notices() -> str:
    parts = ["Third-party notices for BlockRender.dll\n", "=" * 40 + "\n\n"]
    for name, path in THIRD_PARTY:
        text = (REPO / path).read_text(encoding="utf-8").strip()
        parts += [f"{name}\n", "-" * len(name) + "\n\n", text + "\n\n\n"]
    return "".join(parts)


def main() -> None:
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    build, jar, version = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
    out = Path(sys.argv[4]) if len(sys.argv) == 5 else REPO / "release" / "out"
    out.mkdir(parents=True, exist_ok=True)

    files = {
        f"{PLUGIN_DIR}/BlockRender.dll": build / "BlockRender.dll",
        f"{PLUGIN_DIR}/shaders/blocks.hlsl": PLUGIN / "shaders" / "blocks.hlsl",
        f"{PLUGIN_DIR}/Scripts/BlockRender.reds": PLUGIN / "scripts" / "BlockRender.reds",
        f"{PLUGIN_DIR}/build_atlas.py": PLUGIN / "tools" / "build_atlas.py",
        f"{PLUGIN_DIR}/make_atlas.bat": REPO / "release" / "make_atlas.bat",
        f"{PLUGIN_DIR}/LICENSE.txt": REPO / "LICENSE",
        f"{HUD_DIR}/init.lua": PLUGIN / "tools" / "blockrender_hud" / "init.lua",
    }
    game_zip = out / f"CyberCraft-{version}.zip"
    with zipfile.ZipFile(game_zip, "w", zipfile.ZIP_DEFLATED) as z:
        for name, source in files.items():
            z.write(source, name)
        z.writestr(f"{PLUGIN_DIR}/THIRD_PARTY_NOTICES.txt", third_party_notices())
    print(f"{game_zip.name}: {len(files) + 1} files")

    shutil.copy2(jar, out / jar.name)
    print(f"{jar.name}: copied")

    pdb_zip = out / f"BlockRender-{plugin_version()}-pdb.zip"
    with zipfile.ZipFile(pdb_zip, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(build / "BlockRender.pdb", "BlockRender.pdb")
    print(f"{pdb_zip.name}: written")


if __name__ == "__main__":
    main()
