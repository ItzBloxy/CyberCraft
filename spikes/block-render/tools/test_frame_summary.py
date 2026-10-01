import copy

from frame_summary import projection_layout, summarise

# A reversed, infinite D3D projection for row vectors: view x right, y up, z forward (near 0.2).
F = 1.0 / 0.6745  # 1 / tan(34.0 deg), a vertical field of view of 68 deg
PROJECTION = [F / 1.7778, 0, 0, 0,
              0, F, 0, 0,
              0, 0, 0, 1,
              0, 0, 0.2, 0]


def transposed(matrix):
    return [matrix[column * 4 + row] for row in range(4) for column in range(4)]


def target(resource, fmt):
    return {"handle": "0x10", "resource": resource, "format": fmt, "size": [2560, 1440]}


LOG = {
    "pluginVersion": "0.1.0",
    "time": "2026-10-01T18:00:00",
    "frames": [100, 101],
    "device": {
        "seenAtCreation": True,
        "device": "0xabc",
        "gpuApi": {"device": "proxy", "matchesDetour": True, "queue": "proxy", "swapChain": "proxy"},
        "streamline": "proxies",
        "views": {"rtvs": 12, "dsvs": 3, "unknownHandles": 0},
        "patches": {"vtableEntries": 29, "imports": 4},
    },
    "scriptCamera": {"position": [1.0, 2.0, 3.0], "forward": [0.0, 1.0, 0.0], "fov": 68.24},
    "constants": [{
        "frame": 100, "viewport": 0, "version": 2,
        "viewToClip": PROJECTION, "clipToView": [0] * 16, "clipToPrevClip": [0] * 16, "prevClipToClip": [0] * 16,
        "jitter": [0.25, -0.25], "mvecScale": [1, 1], "pinholeOffset": [0, 0],
        "position": [11.0, 2.0, 3.0], "up": [0, 0, 1], "right": [1, 0, 0], "forward": [0.0, 1.0, 0.0],
        "near": 0.2, "far": 10000.0, "fov": 1.19, "fovDegrees": 68.18, "aspect": 1.7778,
        "motionVectorsInvalidValue": 0, "depthInverted": "true", "cameraMotionIncluded": "true",
        "motionVectors3D": "false", "reset": "false", "orthographic": "false", "motionVectorsDilated": "false",
        "motionVectorsJittered": "false",
    }],
    "tags": [
        {"frame": 100, "viewport": 0, "local": False, "type": 0, "name": "depth", "resource": "0x5000",
         "format": "R32G8X24_TYPELESS", "size": [2560, 1440], "flags": "0x2", "mips": 1, "depthOrArraySize": 1,
         "state": "0x80", "lifecycle": "validUntilPresent", "extent": [0, 0, 1707, 960], "list": "0x0"},
        {"frame": 101, "viewport": 0, "local": False, "type": 0, "name": "depth", "resource": "0x5000",
         "format": "R32G8X24_TYPELESS", "size": [2560, 1440], "flags": "0x2", "mips": 1, "depthOrArraySize": 1,
         "state": "0x80", "lifecycle": "validUntilPresent", "extent": [0, 0, 1707, 960], "list": "0x0"},
    ],
    "evaluates": [{"frame": 100, "feature": 0, "viewport": 0, "list": "0x7000", "nativeList": "0x7100"}],
    "events": [
        {"ms": 0.0, "frame": 100, "thread": 7, "what": "frameToken", "detail": "frame 100: the log begins"},
        {"ms": 1.0, "frame": 100, "thread": 9, "what": "submit", "detail": "queue 0x9000 (direct), 1 lists"},
        {"ms": 2.0, "frame": 100, "thread": 9, "what": "evaluate", "detail": "DLSS, frame 100"},
        {"ms": 3.0, "frame": 100, "thread": 9, "what": "submit", "detail": "queue 0x9000 (direct), 2 lists"},
        {"ms": 4.0, "frame": 100, "thread": 9, "what": "present", "detail": "swap chain 0x1, on queue 0x9000"},
    ],
    "submissions": [
        {"ms": 1.0, "frame": 100, "queue": "0x9000", "lists": ["0x6000"], "untracked": 0},
        {"ms": 3.0, "frame": 100, "queue": "0x9000", "lists": ["0x7100", "0x7200"], "untracked": 1},
    ],
    "lists": [{
        "list": "0x7100", "name": "Scene", "frame": 100,
        "passes": [{
            "targets": [target("0x1", "R10G10B10A2_UNORM"), target("0x2", "R10G10B10A2_UNORM"),
                        target("0x3", "R8G8B8A8_UNORM")],
            "depth": target("0x5000", "D32_FLOAT_S8X24_UINT"), "depthIsTagged": True,
            "draws": 0, "indexedDraws": 812, "indirect": 3, "bundles": 0, "dispatches": 0, "clears": 0,
            "barriers": 4, "enhancedBarriers": 0, "events": ["GBuffer"],
        }],
        "transitions": [
            {"resource": "0x1", "firstBefore": "0x0", "firstSubresource": 0, "firstFlags": 1, "after": None},
            {"resource": "0x5000", "firstBefore": "0x10", "firstSubresource": "all", "firstFlags": 0, "after": "0x80"},
        ],
    }],
    "gbufferCandidates": [{"list": "0x7100", "pass": 0}],
}


def text(log=LOG):
    return "\n".join(summarise(log))


def test_projection_layout_reads_row_and_column_vectors():
    layout = projection_layout(PROJECTION)
    assert layout["vectors"] == "row"
    assert layout["forward"] == (2, 1) and layout["right"] == (0, 1) and layout["up"] == (1, 1)
    transposed_layout = projection_layout(transposed(PROJECTION))
    assert transposed_layout["vectors"] == "column"
    assert transposed_layout["forward"] == (2, 1) and transposed_layout["right"] == (0, 1)


def test_projection_layout_rejects_other_matrices():
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    assert projection_layout(identity) is None


def test_summary_compares_the_cameras():
    summary = text()
    assert "Streamline minus script position: [10.0, 0.0, 0.0] (10.0000 m)" in summary
    assert "angle between the forwards: 0.0000 deg" in summary
    assert "viewToClip: row vectors; view +z is forward, +x right, +y up" in summary


def test_summary_lists_each_distinct_tag_once():
    summary = text()
    assert summary.count("depth (global): R32G8X24_TYPELESS 2560x1440") == 1


def test_summary_marks_the_dlss_and_gbuffer_lists():
    summary = text()
    assert "submit: queue 0x9000 (direct), 2 lists, holding the DLSS list and a G-buffer list" in summary
    assert "submit: queue 0x9000 (direct), 1 lists" not in summary
    assert "0x7100 'Scene' frame 100: 1 passes, 815 draws, 0 dispatches [DLSS, G-buffer candidate]" in summary
    assert "target 2: R8G8B8A8_UNORM 2560x1440 (0x3)" in summary
    assert "depth: D32_FLOAT_S8X24_UINT 2560x1440 (0x5000), tagged: True" in summary


def test_summary_shows_the_states_in_the_dlss_and_gbuffer_lists():
    summary = text()
    assert "0x7100 frame 100 [DLSS]\n  depth 0x5000: 0x10 -> 0x80; tagged 0x80" in summary
    assert ("0x7100 frame 100 [G-buffer]: 0x1: 0x0 (subresource 0, flags 1) -> unknown, 0x2: not moved, "
            "0x3: not moved, 0x5000: 0x10 -> 0x80") in summary
    older = copy.deepcopy(LOG)
    del older["lists"][0]["transitions"]  # logs from 0.2.0 and before
    assert "  depth 0x5000: not logged; tagged 0x80" in text(older)


def test_summary_without_a_script_camera_or_candidates():
    log = copy.deepcopy(LOG)
    log["scriptCamera"] = None
    log["gbufferCandidates"] = []
    summary = text(log)
    assert "no script camera was noted (no player?)" in summary
    assert summary.endswith("== G-buffer candidates\nnone")
