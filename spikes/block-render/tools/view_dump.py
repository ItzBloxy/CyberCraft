"""Turns BlockRender's dumped buffers (.dds files) into PNGs you can look at.

    uv run --no-project --with numpy --with pillow tools/view_dump.py <dump folder>

For each name.dds it writes name.png, plus name.alpha.png when the format has an alpha channel, and prints the
format, the size and each channel's range:
- colour targets as they are (8-bit and 10-bit, one channel as grey), HDR ones tone mapped;
- depth stretched between its smallest and largest values, the largest white (the nearest, with reversed depth);
- motion vectors as a flow image: red right, green down, both scaled to the largest motion;
- stencil as a palette, with a count of each value.
"""
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

# DXGI format numbers (dxgiformat.h).
R32G32B32A32_FLOAT = 2
R16G16B16A16_FLOAT = 10
R16G16B16A16_UNORM = 11
R32G32_FLOAT = 16
R10G10B10A2_UNORM = 24
R11G11B10_FLOAT = 26
R8G8B8A8_UNORM = 28
R8G8B8A8_UNORM_SRGB = 29
R16G16_FLOAT = 34
R16G16_UNORM = 35
R16G16_SNORM = 37
R32_FLOAT = 41
R32_UINT = 42
R24_UNORM_X8_TYPELESS = 46
R8G8_UNORM = 49
R16_FLOAT = 54
R16_UNORM = 56
R8_UNORM = 61
R8_UINT = 62
B8G8R8A8_UNORM = 87
B8G8R8A8_UNORM_SRGB = 91

NAMES = {value: name for name, value in globals().items() if name.isupper() and isinstance(value, int)}


def read_dds(path):
    """(format, texels as a 2D numpy array of rows, raw bytes) from a DDS file with a DX10 header."""
    data = Path(path).read_bytes()
    if data[:4] != b"DDS " or data[84:88] != b"DX10":
        raise ValueError(f"{path} isn't a DDS file with a DX10 header")
    height, width, row_bytes = struct.unpack_from("<3I", data, 12)
    dxgi_format = struct.unpack_from("<I", data, 128)[0]
    pixels = data[148:148 + row_bytes * height]
    return dxgi_format, width, height, np.frombuffer(pixels, dtype=np.uint8).reshape(height, row_bytes)


def float11(bits, mantissa_bits):
    """The unsigned small floats of R11G11B10_FLOAT: 5 exponent bits over 6 (or 5) mantissa bits."""
    exponent = (bits >> mantissa_bits) & 31
    mantissa = bits & ((1 << mantissa_bits) - 1)
    scale = float(1 << mantissa_bits)
    normal = np.ldexp(1.0 + mantissa / scale, exponent.astype(np.int32) - 15)
    denormal = np.ldexp(mantissa / scale, -14)
    return np.where(exponent == 0, denormal, np.where(exponent == 31, np.inf, normal))


def decode(dxgi_format, width, height, rows):
    """A float array (height, width, channels) and what kind of image it is: colour, hdr, depth, motion or stencil."""
    raw = rows.reshape(height, -1)
    if dxgi_format in (R8G8B8A8_UNORM, R8G8B8A8_UNORM_SRGB):
        return raw.reshape(height, width, 4) / 255.0, "colour"
    if dxgi_format in (B8G8R8A8_UNORM, B8G8R8A8_UNORM_SRGB):
        return raw.reshape(height, width, 4)[:, :, [2, 1, 0, 3]] / 255.0, "colour"
    if dxgi_format == R10G10B10A2_UNORM:
        words = raw.view("<u4").reshape(height, width)
        channels = [(words >> shift) & mask for shift, mask in ((0, 1023), (10, 1023), (20, 1023), (30, 3))]
        return np.stack([channels[0] / 1023.0, channels[1] / 1023.0, channels[2] / 1023.0, channels[3] / 3.0], axis=-1), "colour"
    if dxgi_format == R11G11B10_FLOAT:
        words = raw.view("<u4").reshape(height, width)
        return np.stack([float11(words & 2047, 6), float11((words >> 11) & 2047, 6), float11((words >> 22) & 1023, 5)], axis=-1), "hdr"
    if dxgi_format == R16G16B16A16_FLOAT:
        return raw.view("<f2").reshape(height, width, 4).astype(np.float64), "hdr"
    if dxgi_format == R16G16B16A16_UNORM:
        return raw.view("<u2").reshape(height, width, 4) / 65535.0, "colour"
    if dxgi_format == R32G32B32A32_FLOAT:
        return raw.view("<f4").reshape(height, width, 4).astype(np.float64), "hdr"
    if dxgi_format == R16G16_FLOAT:
        return raw.view("<f2").reshape(height, width, 2).astype(np.float64), "motion"
    if dxgi_format == R16G16_SNORM:
        return np.maximum(raw.view("<i2").reshape(height, width, 2) / 32767.0, -1.0), "motion"
    if dxgi_format == R16G16_UNORM:
        return raw.view("<u2").reshape(height, width, 2) / 65535.0, "motion"
    if dxgi_format == R32G32_FLOAT:
        return raw.view("<f4").reshape(height, width, 2).astype(np.float64), "motion"
    if dxgi_format == R32_FLOAT:
        return raw.view("<f4").reshape(height, width, 1).astype(np.float64), "depth"
    if dxgi_format == R24_UNORM_X8_TYPELESS:
        return ((raw.view("<u4").reshape(height, width, 1) & 0xFFFFFF) / float(0xFFFFFF)), "depth"
    if dxgi_format == R16_UNORM:
        return raw.view("<u2").reshape(height, width, 1) / 65535.0, "depth"
    if dxgi_format == R16_FLOAT:
        return raw.view("<f2").reshape(height, width, 1).astype(np.float64), "depth"
    if dxgi_format == R32_UINT:
        return raw.view("<u4").reshape(height, width, 1).astype(np.float64), "stencil"
    if dxgi_format == R8_UINT:
        return raw.reshape(height, width, 1).astype(np.float64), "stencil"
    if dxgi_format == R8_UNORM:
        return raw.reshape(height, width, 1) / 255.0, "colour"
    if dxgi_format == R8G8_UNORM:
        return raw.reshape(height, width, 2) / 255.0, "colour"
    raise ValueError(f"format {NAMES.get(dxgi_format, dxgi_format)} isn't supported")


def to_rgb(values, kind):
    """8-bit RGB for an image of each kind."""
    if kind == "colour":
        channels = values.shape[2]
        if channels >= 3:
            rgb = values[:, :, :3]
        elif channels == 2:
            rgb = np.concatenate([values, np.zeros_like(values[:, :, :1])], axis=-1)
        else:
            rgb = np.repeat(values, 3, axis=-1)  # one channel: grey
        return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if kind == "hdr":
        rgb = np.nan_to_num(values[:, :, :3], nan=0.0, posinf=1e4)
        rgb = np.clip(rgb, 0.0, None)
        mapped = (rgb / (1.0 + rgb)) ** (1.0 / 2.2)  # Reinhard, then a display gamma
        return np.clip(mapped * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if kind == "depth":
        depth = values[:, :, 0]
        finite = depth[np.isfinite(depth)]
        low, high = (finite.min(), finite.max()) if finite.size else (0.0, 1.0)
        scaled = (depth - low) / (high - low) if high > low else np.zeros_like(depth)
        grey = np.clip(scaled * 255.0 + 0.5, 0, 255).astype(np.uint8)
        return np.repeat(grey[:, :, None], 3, axis=-1)
    if kind == "motion":
        motion = np.nan_to_num(values[:, :, :2])
        largest = np.abs(motion).max() or 1.0
        rgb = np.zeros(motion.shape[:2] + (3,))
        rgb[:, :, 0] = 0.5 + 0.5 * motion[:, :, 0] / largest
        rgb[:, :, 1] = 0.5 + 0.5 * motion[:, :, 1] / largest
        rgb[:, :, 2] = 0.5
        return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if kind == "stencil":
        value = values[:, :, 0].astype(np.int64) & 0xFF
        rgb = np.stack([(value * 97) % 256, (value * 57) % 256, (value * 151) % 256], axis=-1)
        rgb[value == 0] = 0
        return rgb.astype(np.uint8)
    raise ValueError(kind)


def describe(path, dxgi_format, width, height, values, kind):
    ranges = ", ".join(f"{np.nanmin(values[:, :, c]):.4g}..{np.nanmax(values[:, :, c]):.4g}" for c in range(values.shape[2]))
    line = f"{Path(path).name}: {NAMES.get(dxgi_format, dxgi_format)}, {width} x {height}, {kind}, channels {ranges}"
    if kind == "stencil":
        unique, counts = np.unique(values[:, :, 0].astype(np.int64), return_counts=True)
        line += "; values " + ", ".join(f"0x{u:02x}: {c}" for u, c in zip(unique, counts))
    return line


def convert(path):
    """Writes the PNG(s) for one .dds file and returns its summary line."""
    path = Path(path)
    dxgi_format, width, height, rows = read_dds(path)
    values, kind = decode(dxgi_format, width, height, rows)
    Image.fromarray(to_rgb(values, kind), "RGB").save(path.with_suffix(".png"))
    if kind == "colour" and values.shape[2] == 4:
        alpha = np.clip(values[:, :, 3] * 255.0 + 0.5, 0, 255).astype(np.uint8)
        Image.fromarray(alpha, "L").save(path.with_suffix(".alpha.png"))
    return describe(path, dxgi_format, width, height, values, kind)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    files = sorted(Path(sys.argv[1]).glob("*.dds"))
    if not files:
        raise SystemExit(f"no .dds files in {sys.argv[1]}")
    for path in files:
        print(convert(path))


if __name__ == "__main__":
    main()
