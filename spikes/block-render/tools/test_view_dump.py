"""Tests for view_dump.py, with DDS files laid out as the plugin's DdsWriter writes them.

    uv run --no-project --with numpy --with pillow --with pytest pytest tools/test_view_dump.py
"""
import struct

import numpy as np
import pytest
from PIL import Image

import view_dump as vd


def dds(path, dxgi_format, width, height, texels):
    """A DDS file as src/log/DdsWriter.cpp writes one: packed rows after the DX10 header."""
    texels = bytes(texels)
    row_bytes = len(texels) // height
    header = bytearray(b"DDS ")
    header += struct.pack("<7I", 124, 0x100F, height, width, row_bytes, 0, 0) + bytes(44)
    header += struct.pack("<2I", 32, 4) + b"DX10" + bytes(20)
    header += struct.pack("<5I", 0x1000, 0, 0, 0, 0)
    header += struct.pack("<5I", dxgi_format, 3, 0, 1, 0)
    path.write_bytes(bytes(header) + texels)
    return path


def decoded(path):
    dxgi_format, width, height, rows = vd.read_dds(path)
    return vd.decode(dxgi_format, width, height, rows)


def test_rgba8_and_bgra8(tmp_path):
    values, kind = decoded(dds(tmp_path / "a.dds", vd.R8G8B8A8_UNORM, 2, 1, [255, 0, 0, 255, 0, 0, 255, 128]))
    assert kind == "colour"
    assert values[0, 0].tolist() == [1.0, 0.0, 0.0, 1.0]
    assert values[0, 1, 3] == pytest.approx(128 / 255)
    values, _ = decoded(dds(tmp_path / "b.dds", vd.B8G8R8A8_UNORM, 1, 1, [255, 0, 0, 255]))
    assert values[0, 0, :3].tolist() == [0.0, 0.0, 1.0]  # blue came first


def test_r10g10b10a2(tmp_path):
    word = 1023 | (512 << 10) | (0 << 20) | (3 << 30)
    values, _ = decoded(dds(tmp_path / "n.dds", vd.R10G10B10A2_UNORM, 1, 1, struct.pack("<I", word)))
    assert values[0, 0, 0] == pytest.approx(1.0)
    assert values[0, 0, 1] == pytest.approx(512 / 1023)
    assert values[0, 0, 2] == 0.0
    assert values[0, 0, 3] == 1.0


def test_r11g11b10_float(tmp_path):
    # 1.0 is exponent 15, mantissa 0 in both widths; 0.5 is exponent 14.
    one11 = 15 << 6
    half11 = 14 << 6
    two10 = 16 << 5
    word = one11 | (half11 << 11) | (two10 << 22)
    values, kind = decoded(dds(tmp_path / "h.dds", vd.R11G11B10_FLOAT, 1, 1, struct.pack("<I", word)))
    assert kind == "hdr"
    assert values[0, 0].tolist() == [1.0, 0.5, 2.0]


def test_half_floats(tmp_path):
    texels = np.array([[1.5, -2.0]], dtype="<f2").tobytes()
    values, kind = decoded(dds(tmp_path / "m.dds", vd.R16G16_FLOAT, 1, 1, texels))
    assert kind == "motion"
    assert values[0, 0].tolist() == [1.5, -2.0]
    hdr = np.array([[4.0, 0.0, 1.0, 1.0]], dtype="<f2").tobytes()
    values, kind = decoded(dds(tmp_path / "c.dds", vd.R16G16B16A16_FLOAT, 1, 1, hdr))
    assert kind == "hdr" and values[0, 0, 0] == 4.0


def test_depth_formats(tmp_path):
    values, kind = decoded(dds(tmp_path / "d.dds", vd.R32_FLOAT, 2, 1, np.array([0.25, 1.0], dtype="<f4").tobytes()))
    assert kind == "depth" and values[0, :, 0].tolist() == [0.25, 1.0]
    values, _ = decoded(dds(tmp_path / "e.dds", vd.R24_UNORM_X8_TYPELESS, 1, 1, struct.pack("<I", 0xAB000000 | 0xFFFFFF)))
    assert values[0, 0, 0] == pytest.approx(1.0)  # the stencil byte above the depth is ignored
    values, _ = decoded(dds(tmp_path / "f.dds", vd.R16_UNORM, 1, 1, struct.pack("<H", 65535)))
    assert values[0, 0, 0] == 1.0


def test_stencil_counts_each_value(tmp_path):
    path = dds(tmp_path / "s.dds", vd.R8_UINT, 4, 1, [0x15, 0x15, 0xA0, 0])
    line = vd.convert(path)
    assert "R8_UINT" in line and "0x15: 2" in line and "0xa0: 1" in line
    assert Image.open(path.with_suffix(".png")).size == (4, 1)


def test_convert_writes_pngs(tmp_path):
    path = dds(tmp_path / "albedo.dds", vd.R8G8B8A8_UNORM, 2, 2, [10, 20, 30, 40] * 4)
    line = vd.convert(path)
    assert "2 x 2" in line
    assert np.asarray(Image.open(tmp_path / "albedo.png"))[0, 0].tolist() == [10, 20, 30]
    assert np.asarray(Image.open(tmp_path / "albedo.alpha.png"))[0, 0] == 40
    depth = dds(tmp_path / "depth.dds", vd.R32_FLOAT, 2, 1, np.array([0.0, 1.0], dtype="<f4").tobytes())
    vd.convert(depth)
    assert np.asarray(Image.open(tmp_path / "depth.png"))[0, :, 0].tolist() == [0, 255]


def test_one_channel_colour_is_grey(tmp_path):
    # The second decal set's R8_UNORM targets: the third run in the game found them crashing the viewer.
    path = dds(tmp_path / "mask.dds", vd.R8_UNORM, 2, 1, [0, 255])
    assert "R8_UNORM" in vd.convert(path)
    assert np.asarray(Image.open(tmp_path / "mask.png")).tolist() == [[[0, 0, 0], [255, 255, 255]]]


def test_unknown_formats_are_named(tmp_path):
    with pytest.raises(ValueError, match="71"):
        decoded(dds(tmp_path / "x.dds", 71, 4, 1, bytes(8)))  # BC1
    junk = tmp_path / "junk.dds"
    junk.write_bytes(b"not a dds file" * 20)
    with pytest.raises(ValueError, match="DX10"):
        vd.read_dds(junk)
