"""Summarise a BlockRender frame log (frame.json) against step 1 of spike 3: the device, the camera, the tags, the
order of events, the command lists and the G-buffer candidates (see the spec's "Step 1: the frame log").

    uv run --no-project python tools/frame_summary.py <frame.json>
"""
import json
import math
import sys


def sign(value):
    return -1 if value < 0 else 1


def rows_of(matrix):
    return [list(matrix[i * 4:(i + 1) * 4]) for i in range(4)]


def projection_layout(matrix):
    """How viewToClip maps view space to clip space, read as Camera.cpp's FindProjectionLayout reads it: row or column
    vectors, and which view axis is forward, right and up, with signs. None if it isn't a perspective projection."""

    def perspective(rows):
        ones = 0
        for axis in range(3):
            w = abs(rows[axis][3])
            if abs(w - 1.0) <= 1e-3:
                ones += 1
            elif w > 1e-3:
                return False
        return ones == 1 and abs(rows[3][3]) <= 1e-3

    rows = rows_of(matrix)
    vectors = "row"
    if not perspective(rows):
        rows = [list(column) for column in zip(*rows)]
        if not perspective(rows):
            return None
        vectors = "column"
    forward = next(axis for axis in range(3) if abs(abs(rows[axis][3]) - 1.0) <= 1e-3)
    first, second = (forward + 1) % 3, (forward + 2) % 3
    right = first if abs(rows[first][0]) >= abs(rows[second][0]) else second
    up = second if right == first else first
    return {
        "vectors": vectors,
        "forward": (forward, sign(rows[forward][3])),
        "right": (right, sign(rows[right][0])),
        "up": (up, sign(rows[up][1])),
        "rows": rows,
    }


def angle_degrees(a, b):
    length = math.sqrt(sum(x * x for x in a)) * math.sqrt(sum(x * x for x in b))
    if length == 0:
        return float("nan")
    cosine = max(-1.0, min(1.0, sum(x * y for x, y in zip(a, b)) / length))
    return math.degrees(math.acos(cosine))


def axis_name(axis_and_sign):
    axis, direction = axis_and_sign
    return f"{'+' if direction > 0 else '-'}{'xyz'[axis]}"


def target_text(target):
    if target is None:
        return "none"
    if target.get("unknown"):
        return f"unknown handle {target['handle']}"
    text = f"{target['format']} {target['size'][0]}x{target['size'][1]} ({target['resource']})"
    if "readOnly" in target:
        text += f", read-only {target['readOnly']}"
    return text


def state_text(entry, resource):
    """A resource's states in a logged list: its first transition's state before, and the state after its last."""
    if "transitions" not in entry:
        return "not logged"
    for transition in entry["transitions"]:
        if transition["resource"] == resource:
            first = transition["firstBefore"]
            notes = []
            if transition["firstSubresource"] != "all":
                notes.append(f"subresource {transition['firstSubresource']}")
            if transition["firstFlags"]:
                notes.append(f"flags {transition['firstFlags']}")
            if notes:
                first += f" ({', '.join(notes)})"
            return f"{first} -> {transition['after'] or 'unknown'}"
    return "not moved"


def summarise(log):
    out = []
    add = out.append
    add(f"BlockRender {log['pluginVersion']}, logged {log['time']}, frames {log['frames'][0]} to {log['frames'][1]}")

    device = log["device"]
    gpu_api = device["gpuApi"]
    add("")
    add("== Device")
    add(f"seen at creation: {device['seenAtCreation']}; device {device['device']}")
    add(f"GpuApi: device {gpu_api['device']} (matches the detour's: {gpu_api['matchesDetour']}), queue {gpu_api['queue']}, "
        f"swap chain {gpu_api['swapChain']}")
    add(f"Streamline: {device['streamline']}; patches: {device['patches']['vtableEntries']} vtable entries, "
        f"{device['patches']['imports']} imports")
    add(f"views known: {device['views']['rtvs']} RTVs, {device['views']['dsvs']} DSVs; "
        f"{device['views']['unknownHandles']} bound handles unknown")

    add("")
    add("== Camera")
    evaluated = {(e["frame"], e["viewport"]) for e in log["evaluates"]}
    constants = [c for c in log["constants"] if (c["frame"], c["viewport"]) in evaluated] or log["constants"]
    if not constants:
        add("no constants were logged")
    for c in constants[:2]:
        add(f"frame {c['frame']}, viewport {c['viewport']}, struct version {c['version']}")
        add(f"  position {c['position']}, forward {c['forward']}, right {c['right']}, up {c['up']}")
        # Streamline documents cameraFOV in radians, but the game gives degrees (68.24 at the main menu): print it as is.
        add(f"  near {c['near']}, far {c['far']}, fov {c['fov']} as given, aspect {c['aspect']:.4f}, "
            f"depthInverted {c['depthInverted']}, jitter {c['jitter']}, mvecScale {c['mvecScale']}")
        add(f"  cameraMotionIncluded {c['cameraMotionIncluded']}, motionVectors3D {c['motionVectors3D']}, "
            f"motionVectorsDilated {c['motionVectorsDilated']}, motionVectorsJittered {c['motionVectorsJittered']}, "
            f"reset {c['reset']}, orthographic {c['orthographic']}")
        layout = projection_layout(c["viewToClip"])
        if layout is None:
            add(f"  viewToClip isn't a perspective projection Camera.cpp understands: {c['viewToClip']}")
        else:
            rows = layout["rows"]
            forward = layout["forward"][0]
            add(f"  viewToClip: {layout['vectors']} vectors; view {axis_name(layout['forward'])} is forward, "
                f"{axis_name(layout['right'])} right, {axis_name(layout['up'])} up")
            add(f"  rows (as row vectors): {rows}")
            add(f"  terms that would carry jitter: {rows[forward][0]}, {rows[forward][1]}; depth terms: "
                f"{rows[forward][2]} (forward), {rows[3][2]} (constant)")
    script = log.get("scriptCamera")
    if script and constants:
        c = constants[0]
        offset = [p - s for p, s in zip(c["position"], script["position"])]
        add(f"script camera: position {script['position']}, forward {script['forward']}, fov {script['fov']} deg")
        add(f"  Streamline minus script position: {[round(x, 4) for x in offset]} "
            f"({math.sqrt(sum(x * x for x in offset)):.4f} m)")
        add(f"  angle between the forwards: {angle_degrees(c['forward'], script['forward']):.4f} deg")
    elif not script:
        add("no script camera was noted (no player?)")

    add("")
    add("== Tags (each distinct one)")
    seen = set()
    for tag in log["tags"]:
        size = f"{tag['size'][0]}x{tag['size'][1]}" if "size" in tag else "?"
        key = (tag["name"], tag["local"], tag.get("format", "?"), size, tuple(tag["extent"]), tag["state"], tag["lifecycle"])
        if key in seen:
            continue
        seen.add(key)
        add(f"{tag['name']} ({'local' if tag['local'] else 'global'}): {tag.get('format', '?')} {size}, flags "
            f"{tag.get('flags', '?')}, extent {tag['extent']} (top, left, width, height), state {tag['state']}, "
            f"{tag['lifecycle']}, resource {tag['resource']}")

    add("")
    add("== Evaluates")
    for e in log["evaluates"]:
        add(f"frame {e['frame']}, feature {e['feature']}, viewport {e['viewport']}, list {e['list']}, native {e['nativeList']}")

    lists = {entry["list"]: entry for entry in log["lists"]}
    candidates = {(c["list"], c["pass"]) for c in log["gbufferCandidates"]}
    candidate_lists = {c["list"] for c in log["gbufferCandidates"]}
    native_lists = {e["nativeList"] for e in log["evaluates"]}

    add("")
    add("== Order of events (Streamline calls, presents, and the submissions holding the DLSS or G-buffer lists)")
    submissions = iter(log["submissions"])  # the log adds each submit event and its submission together, in order
    for event in log["events"]:
        if event["what"] == "submit":
            submission = next(submissions, None)
            holds = []
            if submission:
                if native_lists & set(submission["lists"]):
                    holds.append("the DLSS list")
                if candidate_lists & set(submission["lists"]):
                    holds.append("a G-buffer list")
            if not holds:
                continue
            add(f"{event['ms']:9.3f} ms  frame {event['frame']}  thread {event['thread']}  submit: {event['detail']}, "
                f"holding {' and '.join(holds)}")
            continue
        add(f"{event['ms']:9.3f} ms  frame {event['frame']}  thread {event['thread']}  {event['what']}: {event['detail']}")

    add("")
    add("== Submissions")
    queues = {}
    for s in log["submissions"]:
        queues.setdefault(s["queue"], [0, 0, 0])
        queues[s["queue"]][0] += 1
        queues[s["queue"]][1] += len(s["lists"])
        queues[s["queue"]][2] += s["untracked"]
    for queue, (count, total, untracked) in queues.items():
        add(f"queue {queue}: {count} submissions, {total} lists, {untracked} untracked")

    add("")
    add("== Command lists")
    totals = {"draws": 0, "indexedDraws": 0, "indirect": 0, "dispatches": 0, "barriers": 0, "enhancedBarriers": 0}
    render_passes = 0
    for entry in log["lists"]:
        passes = entry["passes"]
        for p in passes:
            for key in totals:
                totals[key] += p[key]
            render_passes += 1 if p.get("renderPass") else 0
        draws = sum(p["draws"] + p["indexedDraws"] + p["indirect"] for p in passes)
        dispatches = sum(p["dispatches"] for p in passes)
        marks = []
        if entry["list"] in native_lists:
            marks.append("DLSS")
        if entry["list"] in candidate_lists:
            marks.append("G-buffer candidate")
        add(f"{entry['list']} '{entry['name']}' frame {entry['frame']}: {len(passes)} passes, {draws} draws, {dispatches} "
            f"dispatches{(' [' + ', '.join(marks) + ']') if marks else ''}")
    add(f"in all: {len(log['lists'])} lists; {totals}; {render_passes} render passes")

    add("")
    add("== States in the DLSS and G-buffer lists (before the first transition -> after the last)")
    tagged = {}
    for tag in log["tags"]:
        if tag["resource"] != "0x0":
            tagged.setdefault(tag["resource"], (tag["name"], tag["state"]))
    for entry in log["lists"]:
        if entry["list"] in native_lists:
            add(f"{entry['list']} frame {entry['frame']} [DLSS]")
            for resource, (name, state) in tagged.items():
                add(f"  {name} {resource}: {state_text(entry, resource)}; tagged {state}")
        if entry["list"] in candidate_lists:
            resources = []
            for index, p in enumerate(entry["passes"]):
                if (entry["list"], index) in candidates:
                    for t in p["targets"] + [p["depth"]]:
                        if t and not t.get("unknown") and t["resource"] not in resources:
                            resources.append(t["resource"])
            states = ", ".join(f"{resource}: {state_text(entry, resource)}" for resource in resources)
            add(f"{entry['list']} frame {entry['frame']} [G-buffer]: {states}")

    add("")
    add("== G-buffer candidates")
    if not candidates:
        add("none")
    for list_address, index in sorted(candidates):
        entry = lists.get(list_address)
        if entry is None:
            add(f"{list_address} pass {index}: the list isn't in the log")
            continue
        p = entry["passes"][index]
        add(f"{list_address} '{entry['name']}' pass {index}: {p['draws']} draws, {p['indexedDraws']} indexed, "
            f"{p['indirect']} indirect, {p['clears']} clears, events {p['events']}")
        for i, target in enumerate(p["targets"]):
            add(f"  target {i}: {target_text(target)}")
        add(f"  depth: {target_text(p['depth'])}, tagged: {p['depthIsTagged']}")
    return out


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    with open(argv[1], encoding="utf-8") as file:
        log = json.load(file)
    print("\n".join(summarise(log)))


if __name__ == "__main__":
    main(sys.argv)
