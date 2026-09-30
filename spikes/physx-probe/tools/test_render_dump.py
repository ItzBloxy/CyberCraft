"""Tests for render_dump.py:  uv run --no-project --with numpy --with pillow --with pytest pytest tools/test_render_dump.py -q"""

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import render_dump as rd  # noqa: E402

OBJ = """# PhysXProbe collision dump
mtllib collision.mtl
o static_trimesh
usemtl static_trimesh
v 0.000 0.000 0.000
v 1.000 0.000 0.000
v 0.000 0.000 -1.000
f 1 2 3
o trigger_box
usemtl trigger_box
v 0.000 1.000 0.000
v 1.000 1.000 0.000
v 0.000 1.000 -1.000
f 4 5 6
"""


def test_load_obj_groups_faces(tmp_path):
    path = tmp_path / "collision.obj"
    path.write_text(OBJ)
    groups = rd.load_obj(path)
    assert sorted(groups) == ["static_trimesh", "trigger_box"]
    assert groups["static_trimesh"].shape == (1, 3, 3)
    assert groups["trigger_box"][0][2].tolist() == [0.0, 1.0, -1.0]


def test_hidden_groups():
    assert rd.is_hidden("trigger_box")
    assert rd.is_hidden("s1_queryonly_trimesh")
    assert not rd.is_hidden("static_trimesh")
    assert not rd.is_hidden("s0_static_trimesh")


def test_mc_from_world():
    assert rd.mc_from_world([1.0, 2.0, 3.0]).tolist() == [1.0, 3.0, -2.0]


def test_clip_near():
    front = np.array([[0, 0, 1.0], [1, 0, 1.0], [0, 1, 1.0]])
    behind = np.array([[0, 0, -1.0], [1, 0, -1.0], [0, 1, -1.0]])
    crossing = np.array([[0, 0, -1.0], [1, 0, 1.0], [0, 1, 1.0]])
    assert len(rd.clip_near(front)) == 3
    assert len(rd.clip_near(behind)) == 0
    clipped = rd.clip_near(crossing)
    assert len(clipped) == 4
    assert (clipped[:, 2] >= rd.NEAR - 1e-9).all()


def test_render_eye_occludes_the_floor_behind_a_wall():
    # A big floor and a wall 6 m ahead standing on it. Just above the wall's bottom edge, the wall must win.
    # (Sorting triangles far to near got this wrong: floor pieces behind the wall were drawn over its base.)
    floor = np.array([
        [[-20.0, 0.0, -20.0], [20.0, 0.0, -20.0], [20.0, 0.0, 20.0]],
        [[-20.0, 0.0, -20.0], [20.0, 0.0, 20.0], [-20.0, 0.0, 20.0]],
    ])
    wall = np.array([
        [[-2.0, 0.0, -6.0], [2.0, 0.0, -6.0], [2.0, 4.0, -6.0]],
        [[-2.0, 0.0, -6.0], [2.0, 4.0, -6.0], [-2.0, 4.0, -6.0]],
    ])
    groups = {"static_trimesh": floor, "static_box": wall}
    colors = {"static_trimesh": (0.0, 1.0, 0.0), "static_box": (1.0, 0.0, 0.0)}
    image = rd.render_eye(groups, colors, eye=[0.0, 1.7, 0.0], forward=[0.0, 0.0, -1.0], fov_deg=80.0, size=(320, 180))
    # The wall's base is at screen y ~ 120 (for a vertical fov of 80 degrees); check just above it.
    r, g, b = image.getpixel((160, 116))
    assert r > 100 and g == 0, (r, g, b)
    r, g, b = image.getpixel((160, 170))  # the floor in front of the wall
    assert g > 100 and r == 0, (r, g, b)


def test_render_eye_sees_a_wall_straight_ahead():
    # A red wall 3 m in front of the eye (Minecraft -Z is north, straight ahead here), a grey floor below.
    wall = np.array([
        [[-5.0, -2.0, -3.0], [5.0, -2.0, -3.0], [5.0, 8.0, -3.0]],
        [[-5.0, -2.0, -3.0], [5.0, 8.0, -3.0], [-5.0, 8.0, -3.0]],
    ])
    groups = {"static_box": wall}
    colors = {"static_box": (1.0, 0.0, 0.0)}
    image = rd.render_eye(groups, colors, eye=[0.0, 1.7, 0.0], forward=[0.0, 0.0, -1.0], fov_deg=80.0, size=(320, 180))
    r, g, b = image.getpixel((160, 90))
    assert r > 100 and g == 0 and b == 0


def test_render_top_draws_the_floor(tmp_path):
    floor = np.array([[[-10.0, 0.0, -10.0], [10.0, 0.0, -10.0], [10.0, 0.0, 10.0]]])
    image = rd.render_top({"static_trimesh": floor}, {"static_trimesh": (0.0, 1.0, 0.0)}, size=64)
    assert image.getpixel((40, 30))[1] > 100  # inside the triangle: green
    assert image.getpixel((5, 60)) == rd.SKY  # outside it: background
