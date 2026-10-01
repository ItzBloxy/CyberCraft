"""Render a PhysXProbe dump to images, so it can be checked without Blender.

    uv run --no-project --with numpy --with pillow tools/render_dump.py <dump folder> [--all] [--fov 80] [--fov-axis v|h]

Writes top.png (orthographic, from above, north up) and eye.png (from the camera recorded in report.json, or
1.7 m above V's feet looking along V's facing) into the dump folder. collision.obj is in Minecraft axes
(X east, Y up, Z south), relative to V's feet; report.json is in world axes (X east, Y north, Z up).
Both views use a z-buffer, so walls correctly hide what's behind them.
"""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

import numpy as np
from PIL import Image

HIDDEN_KINDS = ("trigger_", "queryonly_")
HIDDEN_GROUPS = ("kinematic_capsule",)  # character controllers; the camera sits inside V's own
NEAR = 0.05
SKY = (22, 24, 30)


def is_hidden(group: str) -> bool:
    """trigger_* and queryonly_* groups and kinematic capsules, with or without a scene prefix such as s1_."""
    name = re.sub(r"^s\d+_", "", group)
    return name.startswith(HIDDEN_KINDS) or name in HIDDEN_GROUPS


def load_mtl(path: Path) -> dict[str, tuple[float, float, float]]:
    colors: dict[str, tuple[float, float, float]] = {}
    name = None
    for line in path.read_text().splitlines():
        parts = line.split()
        if parts[:1] == ["newmtl"]:
            name = parts[1]
        elif parts[:1] == ["Kd"] and name:
            colors[name] = (float(parts[1]), float(parts[2]), float(parts[3]))
    return colors


def load_obj(path: Path) -> dict[str, np.ndarray]:
    """Triangles per group: {group: array of shape (n, 3, 3)}."""
    vertices: list[tuple[float, float, float]] = []
    faces: dict[str, list[tuple[int, int, int]]] = {}
    group = "default"
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif parts[0] == "o":
            group = parts[1]
            faces.setdefault(group, [])
        elif parts[0] == "f":
            faces.setdefault(group, []).append(tuple(int(p.split("/")[0]) - 1 for p in parts[1:4]))
    table = np.array(vertices, dtype=np.float64).reshape(-1, 3)
    return {name: table[np.array(tris, dtype=np.int64)] for name, tris in faces.items() if tris}


def mc_from_world(v) -> np.ndarray:
    """World (X east, Y north, Z up) to Minecraft axes (X east, Y up, Z south)."""
    return np.array([v[0], v[2], -v[1]], dtype=np.float64)


def clip_near(points: np.ndarray, near: float = NEAR) -> np.ndarray:
    """Clip a camera-space polygon (depth in column 2) to depth >= near. Returns 0, 3 or 4+ points."""
    out = []
    count = len(points)
    for i in range(count):
        a, b = points[i], points[(i + 1) % count]
        a_in, b_in = a[2] >= near, b[2] >= near
        if a_in:
            out.append(a)
        if a_in != b_in:
            t = (near - a[2]) / (b[2] - a[2])
            out.append(a + (b - a) * t)
    return np.array(out) if len(out) >= 3 else np.zeros((0, 3))


def shade(color, normal: np.ndarray, light: np.ndarray, ambient: float) -> np.ndarray:
    """Two-sided Lambert shading of a flat colour, as 0-255 RGB."""
    length = np.linalg.norm(normal)
    lambert = abs(float(np.dot(normal / length, light))) if length > 0 else 0.0
    k = ambient + (1.0 - ambient) * lambert
    return np.clip(np.round(255.0 * np.asarray(color) * k), 0, 255).astype(np.uint8)


class Canvas:
    """An RGB image with a depth buffer. Nearer (smaller) depths win."""

    def __init__(self, width: int, height: int):
        self.width, self.height = width, height
        self.rgb = np.empty((height, width, 3), dtype=np.uint8)
        self.rgb[:] = SKY
        self.depth = np.full((height, width), np.inf)

    def triangle(self, xy: np.ndarray, depth: np.ndarray, color: np.ndarray, perspective: bool) -> None:
        """Fill a screen-space triangle (xy: 3x2 pixels, depth: 3 values) at pixel centres."""
        x_min = max(int(math.floor(xy[:, 0].min())), 0)
        x_max = min(int(math.ceil(xy[:, 0].max())), self.width - 1)
        y_min = max(int(math.floor(xy[:, 1].min())), 0)
        y_max = min(int(math.ceil(xy[:, 1].max())), self.height - 1)
        if x_min > x_max or y_min > y_max:
            return
        (x0, y0), (x1, y1), (x2, y2) = xy
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-12:
            return
        xs, ys = np.meshgrid(np.arange(x_min, x_max + 1) + 0.5, np.arange(y_min, y_max + 1) + 0.5)
        w0 = ((x1 - xs) * (y2 - ys) - (x2 - xs) * (y1 - ys)) / area
        w1 = ((x2 - xs) * (y0 - ys) - (x0 - xs) * (y2 - ys)) / area
        w2 = 1.0 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if perspective:  # 1/depth is linear in screen space
            z = 1.0 / (w0 / depth[0] + w1 / depth[1] + w2 / depth[2])
        else:
            z = w0 * depth[0] + w1 * depth[1] + w2 * depth[2]
        window = self.depth[y_min:y_max + 1, x_min:x_max + 1]
        nearer = inside & (z < window)
        window[nearer] = z[nearer]
        self.rgb[y_min:y_max + 1, x_min:x_max + 1][nearer] = color

    def image(self) -> Image.Image:
        return Image.fromarray(self.rgb, "RGB")


def render_top(groups, colors, size: int = 1024, extent: float = 32.0) -> Image.Image:
    """Looking straight down on +-extent metres around V; north (-Z) is up."""
    canvas = Canvas(size, size)
    scale = size / (2.0 * extent)
    light = np.array([0.4, 1.0, 0.3]) / np.linalg.norm([0.4, 1.0, 0.3])
    for name, tris in groups.items():
        color = colors.get(name, (0.6, 0.6, 0.6))
        for tri in tris:
            xy = np.stack([(tri[:, 0] + extent) * scale, (tri[:, 2] + extent) * scale], axis=1)
            fill = shade(color, np.cross(tri[1] - tri[0], tri[2] - tri[0]), light, 0.35)
            canvas.triangle(xy, -tri[:, 1], fill, perspective=False)
    return canvas.image()


def camera_basis(forward: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    f = forward / np.linalg.norm(forward)
    up = np.array([0.0, 1.0, 0.0])
    right = np.cross(f, up)
    if np.linalg.norm(right) < 1e-6:
        right = np.array([1.0, 0.0, 0.0])
    right /= np.linalg.norm(right)
    return right, np.cross(right, f), f


def render_eye(groups, colors, eye, forward, fov_deg: float, fov_axis: str = "v", size=(1280, 720)) -> Image.Image:
    """Perspective view from eye along forward (both in Minecraft axes, relative to V's feet)."""
    width, height = size
    aspect = width / height
    if fov_axis == "h":
        tan_h = math.tan(math.radians(fov_deg) / 2.0)
        tan_v = tan_h / aspect
    else:
        tan_v = math.tan(math.radians(fov_deg) / 2.0)
        tan_h = tan_v * aspect
    right, up, f = camera_basis(np.asarray(forward, dtype=np.float64))
    eye = np.asarray(eye, dtype=np.float64)
    light = -f + up * 0.5
    light /= np.linalg.norm(light)
    canvas = Canvas(width, height)
    for name, tris in groups.items():
        color = colors.get(name, (0.6, 0.6, 0.6))
        rel = tris - eye
        cams = np.stack([rel @ right, rel @ up, rel @ f], axis=2)  # (n, 3, 3) camera space
        for tri, cam in zip(tris, cams):
            if (cam[:, 2] < NEAR).all():
                continue  # entirely behind the camera
            polygon = cam if (cam[:, 2] >= NEAR).all() else clip_near(cam)
            if len(polygon) == 0:
                continue
            xy = np.stack([width / 2.0 * (1.0 + polygon[:, 0] / (polygon[:, 2] * tan_h)),
                           height / 2.0 * (1.0 - polygon[:, 1] / (polygon[:, 2] * tan_v))], axis=1)
            fill = shade(color, np.cross(tri[1] - tri[0], tri[2] - tri[0]), light, 0.3)
            for k in range(1, len(polygon) - 1):  # fan-triangulate the clipped polygon
                corners = [0, k, k + 1]
                canvas.triangle(xy[corners], polygon[corners, 2], fill, perspective=True)
    return canvas.image()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dump", type=Path, help="a dumps/<timestamp> folder")
    parser.add_argument("--all", action="store_true", help="include trigger_*, queryonly_* and kinematic_capsule groups")
    parser.add_argument("--fov", type=float, help="field of view in degrees (default: the report's camera, else 80)")
    parser.add_argument("--fov-axis", choices=("v", "h"), default="v", help="whether the field of view is vertical or horizontal")
    args = parser.parse_args()

    report = json.loads((args.dump / "report.json").read_text())
    groups = load_obj(args.dump / "collision.obj")
    colors = load_mtl(args.dump / "collision.mtl")
    if not args.all:
        groups = {name: tris for name, tris in groups.items() if not is_hidden(name)}

    player = report["player"]
    feet = np.array(player["feet"], dtype=np.float64)
    camera = player.get("camera")
    if camera:
        eye = mc_from_world(np.array(camera["position"]) - feet)
        forward = mc_from_world(camera["forward"])
        fov = args.fov or camera["fov"]
    else:
        eye = np.array([0.0, 1.7, 0.0])
        forward = mc_from_world(player["forward"])
        fov = args.fov or 80.0

    render_top(groups, colors).save(args.dump / "top.png")
    render_eye(groups, colors, eye, forward, fov, args.fov_axis).save(args.dump / "eye.png")
    total = sum(len(t) for t in groups.values())
    print(f"rendered {total} triangles in {len(groups)} group(s) to {args.dump / 'top.png'} and {args.dump / 'eye.png'}")


if __name__ == "__main__":
    main()
