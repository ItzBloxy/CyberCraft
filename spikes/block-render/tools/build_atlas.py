"""Builds BlockRender's block atlas from your own Minecraft jar.

    uv run --no-project --with pillow tools/build_atlas.py <minecraft.jar> <output folder>

Writes atlas.bin, which the plugin reads, and atlas.json, the same for people. The textures belong to Mojang, so the
output goes into the game's plugin folder and never into the repo.

atlas.bin is little-endian:
- header, 32 bytes: magic "CPMCATL1", then uint32 width, height, levels, sprite count, block count, 0;
- sprites, 16 bytes each: uint32 x, y, w, h, in level-0 texels;
- blocks, 64 bytes each: char name[32] (zero-padded), uint16 sprite per face (down, up, north, south, west, east),
  uint8 tint red, green, blue, uint8 tinted faces (bit per face, same order), uint8 cut-out (1: has see-through
  texels), 15 zero bytes;
- the texels, RGBA8 row by row, level 0 first; level l is max(1, width >> l) by max(1, height >> l).
"""
import io
import json
import struct
import sys
import zipfile
from pathlib import Path

from PIL import Image

TILE = 16
COLUMNS = 16
LEVELS = 5  # 16, 8, 4, 2 and 1 texels per sprite
FACES = ("down", "up", "north", "south", "west", "east")
GRASS = (0x91, 0xBD, 0x59)  # Minecraft's plains grass colour
FOLIAGE = (0x77, 0xAB, 0x2F)  # Minecraft's plains foliage colour

# name, textures, tint, tinted faces, cut-out. Textures: one name for every face, or a dict with "end" (down and up),
# "side" (the four sides) and any single face, later keys winning.
BLOCKS = [
    ("stone", "stone", None, (), False),
    ("cobblestone", "cobblestone", None, (), False),
    ("mossy_cobblestone", "mossy_cobblestone", None, (), False),
    ("dirt", "dirt", None, (), False),
    ("grass_block", {"down": "dirt", "up": "grass_block_top", "side": "grass_block_side"}, GRASS, ("up",), False),
    ("sand", "sand", None, (), False),
    ("gravel", "gravel", None, (), False),
    ("clay", "clay", None, (), False),
    ("oak_planks", "oak_planks", None, (), False),
    ("spruce_planks", "spruce_planks", None, (), False),
    ("birch_planks", "birch_planks", None, (), False),
    ("oak_log", {"end": "oak_log_top", "side": "oak_log"}, None, (), False),
    ("oak_leaves", "oak_leaves", FOLIAGE, FACES, True),
    ("glass", "glass", None, (), True),
    ("bricks", "bricks", None, (), False),
    ("stone_bricks", "stone_bricks", None, (), False),
    ("white_wool", "white_wool", None, (), False),
    ("red_wool", "red_wool", None, (), False),
    ("blue_wool", "blue_wool", None, (), False),
    ("yellow_wool", "yellow_wool", None, (), False),
    ("lime_wool", "lime_wool", None, (), False),
    ("black_wool", "black_wool", None, (), False),
    ("iron_block", "iron_block", None, (), False),
    ("gold_block", "gold_block", None, (), False),
    ("diamond_block", "diamond_block", None, (), False),
    ("emerald_block", "emerald_block", None, (), False),
    ("lapis_block", "lapis_block", None, (), False),
    ("redstone_block", "redstone_block", None, (), False),
    ("coal_ore", "coal_ore", None, (), False),
    ("iron_ore", "iron_ore", None, (), False),
    ("gold_ore", "gold_ore", None, (), False),
    ("diamond_ore", "diamond_ore", None, (), False),
    ("deepslate", {"end": "deepslate_top", "side": "deepslate"}, None, (), False),
    ("granite", "granite", None, (), False),
    ("diorite", "diorite", None, (), False),
    ("andesite", "andesite", None, (), False),
    ("sandstone", {"down": "sandstone_bottom", "up": "sandstone_top", "side": "sandstone"}, None, (), False),
    ("obsidian", "obsidian", None, (), False),
    ("netherrack", "netherrack", None, (), False),
    ("glowstone", "glowstone", None, (), False),
    ("magma_block", "magma", None, (), False),
    ("snow_block", "snow", None, (), False),
    ("quartz_block", {"down": "quartz_block_bottom", "up": "quartz_block_top", "side": "quartz_block_side"}, None, (), False),
    ("bookshelf", {"end": "oak_planks", "side": "bookshelf"}, None, (), False),
    ("pumpkin", {"end": "pumpkin_top", "side": "pumpkin_side"}, None, (), False),
    ("tnt", {"down": "tnt_bottom", "up": "tnt_top", "side": "tnt_side"}, None, (), False),
    ("crafting_table", {"down": "oak_planks", "up": "crafting_table_top", "side": "crafting_table_side",
                        "north": "crafting_table_front", "east": "crafting_table_front"}, None, (), False),
    ("terracotta", "terracotta", None, (), False),
    ("white_concrete", "white_concrete", None, (), False),
    ("cyan_concrete", "cyan_concrete", None, (), False),
]


def face_textures(textures):
    """The texture name of each face, in FACES order."""
    if isinstance(textures, str):
        return [textures] * 6
    faces = {}
    for key, name in textures.items():
        targets = {"end": ("down", "up"), "side": ("north", "south", "west", "east")}.get(key, (key,))
        for face in targets:
            faces[face] = name
    missing = [face for face in FACES if face not in faces]
    if missing:
        raise ValueError(f"no texture for faces {missing} in {textures}")
    return [faces[face] for face in FACES]


def load_texture(jar, name):
    """A block texture as a 16 x 16 RGBA image; animated textures give their first frame."""
    path = f"assets/minecraft/textures/block/{name}.png"
    try:
        data = jar.read(path)
    except KeyError:
        raise SystemExit(f"error: {path} isn't in the jar") from None
    image = Image.open(io.BytesIO(data)).convert("RGBA")
    if image.width != TILE or image.height < TILE:
        raise SystemExit(f"error: {path} is {image.width} x {image.height}, not {TILE} wide and at least {TILE} high")
    return image.crop((0, 0, TILE, TILE))


def mip_levels(atlas):
    """Level 0 and its box-filtered halvings. Sprites sit on a 16-texel grid, so no level mixes two sprites."""
    levels = [atlas]
    for _ in range(1, LEVELS):
        previous = levels[-1]
        levels.append(previous.resize((max(1, previous.width // 2), max(1, previous.height // 2)), Image.Resampling.BOX))
    return levels


def build(jar_path, out_dir):
    out_dir = Path(out_dir)
    with zipfile.ZipFile(jar_path) as jar:
        sprite_names = []
        for _, textures, _, _, _ in BLOCKS:
            for name in face_textures(textures):
                if name not in sprite_names:
                    sprite_names.append(name)
        rows = (len(sprite_names) + COLUMNS - 1) // COLUMNS
        atlas = Image.new("RGBA", (COLUMNS * TILE, rows * TILE), (0, 0, 0, 0))
        sprites = []
        for index, name in enumerate(sprite_names):
            x, y = (index % COLUMNS) * TILE, (index // COLUMNS) * TILE
            atlas.paste(load_texture(jar, name), (x, y))
            sprites.append({"name": name, "x": x, "y": y, "w": TILE, "h": TILE})

    blocks = []
    for name, textures, tint, tinted, cutout in BLOCKS:
        faces = [sprite_names.index(texture) for texture in face_textures(textures)]
        blocks.append({
            "name": name,
            "faces": dict(zip(FACES, faces)),
            "tint": list(tint or (255, 255, 255)),
            "tintFaces": list(tinted),
            "cutout": cutout,
        })

    levels = mip_levels(atlas)
    out = bytearray(b"CPMCATL1")
    out += struct.pack("<6I", atlas.width, atlas.height, LEVELS, len(sprites), len(blocks), 0)
    for sprite in sprites:
        out += struct.pack("<4I", sprite["x"], sprite["y"], sprite["w"], sprite["h"])
    for block in blocks:
        tinted_bits = sum(1 << FACES.index(face) for face in block["tintFaces"])
        out += block["name"].encode("ascii").ljust(32, b"\0")
        out += struct.pack("<6H", *(block["faces"][face] for face in FACES))
        out += bytes(block["tint"]) + bytes([tinted_bits, 1 if block["cutout"] else 0]) + bytes(15)
    for level in levels:
        out += level.tobytes()

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "atlas.bin").write_bytes(bytes(out))
    summary = {"width": atlas.width, "height": atlas.height, "levels": LEVELS, "sprites": sprites, "blocks": blocks}
    (out_dir / "atlas.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return summary


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    summary = build(sys.argv[1], sys.argv[2])
    print(f"atlas: {summary['width']} x {summary['height']}, {len(summary['sprites'])} sprites, "
          f"{len(summary['blocks'])} blocks, written to {sys.argv[2]}")


if __name__ == "__main__":
    main()
