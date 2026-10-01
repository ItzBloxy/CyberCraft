"""Tests for build_atlas.py, against a small fake jar.

    uv run --no-project --with pillow --with pytest pytest tools/test_build_atlas.py
"""
import io
import struct
import zipfile

import pytest
from PIL import Image

import build_atlas


def png(colour, size=(16, 16)):
    buffer = io.BytesIO()
    Image.new("RGBA", size, colour).save(buffer, "PNG")
    return buffer.getvalue()


def every_texture():
    names = set()
    for _, textures, _, _, _ in build_atlas.BLOCKS:
        names.update(build_atlas.face_textures(textures))
    return sorted(names)


def fake_jar(path, skip=(), overrides=None):
    overrides = overrides or {}
    with zipfile.ZipFile(path, "w") as jar:
        for index, name in enumerate(every_texture()):
            if name in skip:
                continue
            data = overrides.get(name) or png((index % 256, 100, 200, 255))
            jar.writestr(f"assets/minecraft/textures/block/{name}.png", data)
    return path


def read_atlas(path):
    data = (path / "atlas.bin").read_bytes()
    assert data[:8] == b"CPMCATL1"
    width, height, levels, sprite_count, block_count, zero = struct.unpack_from("<6I", data, 8)
    at = 32
    sprites = [struct.unpack_from("<4I", data, at + 16 * i) for i in range(sprite_count)]
    at += 16 * sprite_count
    blocks = []
    for i in range(block_count):
        record = data[at + 64 * i: at + 64 * (i + 1)]
        name = record[:32].rstrip(b"\0").decode()
        faces = struct.unpack_from("<6H", record, 32)
        blocks.append((name, faces, tuple(record[44:47]), record[47], record[48]))
    at += 64 * block_count
    return {"size": (width, height), "levels": levels, "zero": zero, "sprites": sprites, "blocks": blocks,
            "pixels": data[at:]}


def test_face_textures():
    assert build_atlas.face_textures("stone") == ["stone"] * 6
    assert build_atlas.face_textures({"end": "top", "side": "side"}) == ["top", "top", "side", "side", "side", "side"]
    assert build_atlas.face_textures({"end": "a", "side": "b", "north": "c"}) == ["a", "a", "c", "b", "b", "b"]
    with pytest.raises(ValueError):
        build_atlas.face_textures({"side": "b"})


def test_atlas_layout(tmp_path):
    jar = fake_jar(tmp_path / "fake.jar")
    summary = build_atlas.build(jar, tmp_path / "out")
    atlas = read_atlas(tmp_path / "out")
    assert atlas["zero"] == 0
    assert atlas["levels"] == 5
    assert atlas["size"] == (256, 16 * ((len(atlas["sprites"]) + 15) // 16))
    assert len(atlas["blocks"]) == len(build_atlas.BLOCKS)
    assert atlas["sprites"][1] == (16, 0, 16, 16)
    # Every level's texels follow the tables, each half the size of the one before.
    width, height = atlas["size"]
    expected = sum(max(1, width >> level) * max(1, height >> level) * 4 for level in range(5))
    assert len(atlas["pixels"]) == expected
    assert summary["blocks"][0]["name"] == "stone"


def test_blocks_name_their_faces_tints_and_cutouts(tmp_path):
    atlas = read_atlas_after_build(tmp_path)
    blocks = {name: (faces, tint, tinted, cutout) for name, faces, tint, tinted, cutout in atlas["blocks"]}
    names = [name for name, _, _, _, _ in build_atlas.BLOCKS]
    sprite_names = []
    for _, textures, _, _, _ in build_atlas.BLOCKS:
        for name in build_atlas.face_textures(textures):
            if name not in sprite_names:
                sprite_names.append(name)
    log_faces = blocks["oak_log"][0]
    assert sprite_names[log_faces[1]] == "oak_log_top"  # up
    assert sprite_names[log_faces[2]] == "oak_log"  # north
    grass = blocks["grass_block"]
    assert sprite_names[grass[0][0]] == "dirt"
    assert grass[1] == build_atlas.GRASS
    assert grass[2] == 0b000010  # only the top is tinted
    assert blocks["oak_leaves"][2] == 0b111111
    assert blocks["glass"][3] == 1 and blocks["stone"][3] == 0
    assert blocks["stone"][1] == (255, 255, 255)
    table = blocks["crafting_table"][0]
    assert [sprite_names[i] for i in table] == ["oak_planks", "crafting_table_top", "crafting_table_front",
                                                "crafting_table_side", "crafting_table_side", "crafting_table_front"]
    assert len(names) == len(set(names))


def read_atlas_after_build(tmp_path):
    jar = fake_jar(tmp_path / "fake.jar")
    build_atlas.build(jar, tmp_path / "out")
    return read_atlas(tmp_path / "out")


def test_mip_levels_average_each_sprite(tmp_path):
    # Stone: a 16 x 16 checkerboard of black and white averages to grey at level 1 and below.
    checker = Image.new("RGBA", (16, 16))
    checker.putdata([(255, 255, 255, 255) if (x + y) % 2 else (0, 0, 0, 255) for y in range(16) for x in range(16)])
    buffer = io.BytesIO()
    checker.save(buffer, "PNG")
    jar = fake_jar(tmp_path / "fake.jar", overrides={"stone": buffer.getvalue()})
    build_atlas.build(jar, tmp_path / "out")
    atlas = read_atlas(tmp_path / "out")
    width, height = atlas["size"]
    level0 = width * height * 4
    level1 = atlas["pixels"][level0:level0 + 4]  # texel (0, 0) of level 1: stone's top-left 2 x 2
    assert abs(level1[0] - 127) <= 1 and level1[3] == 255


def test_animated_textures_give_their_first_frame(tmp_path):
    frames = Image.new("RGBA", (16, 48), (10, 20, 30, 255))
    frames.paste(Image.new("RGBA", (16, 16), (200, 0, 0, 255)), (0, 0))
    buffer = io.BytesIO()
    frames.save(buffer, "PNG")
    jar = fake_jar(tmp_path / "fake.jar", overrides={"magma": buffer.getvalue()})
    summary = build_atlas.build(jar, tmp_path / "out")
    sprite = next(s for s in summary["sprites"] if s["name"] == "magma")
    atlas = read_atlas(tmp_path / "out")
    width = atlas["size"][0]
    at = (sprite["y"] * width + sprite["x"]) * 4
    assert tuple(atlas["pixels"][at:at + 4]) == (200, 0, 0, 255)


def test_a_missing_texture_is_named(tmp_path):
    jar = fake_jar(tmp_path / "fake.jar", skip=("glass",))
    with pytest.raises(SystemExit) as failure:
        build_atlas.build(jar, tmp_path / "out")
    assert "glass.png" in str(failure.value)
