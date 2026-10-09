"""Spike 9's Part 2: break each new check once. Each mutation is an exact text swap in one file of a clone at Task
17's commit; the step that guards it (its bash block, as the plan has it) must then fail: some `text` of its Expected
paragraph missing from what it printed. The clone is restored after each.

    uv run --no-project python mutate_s9_overlay.py <plan.md> <clone>

The clone needs Part 2's tools (tools/plans/fun_check.lua with FUN_V) and mclink.exe in its build folder.
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import planlib  # noqa: E402

plan, clone = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
steps = {(s.task, s.number): s for s in planlib.read_plan(plan)}
OVERLAY = "spikes/block-render/tools/blockrender_hud/init.lua"
SCRIPT = "spikes/block-render/tools/mclink/pathsrow.txt"
MUTATIONS = [
    # (file, old, new, task, step, what)
    (OVERLAY, 'mob("^ ^ ^4", "cow", nil, "~ ~0.5 ~")', 'mob("^ ^ ^4", "cow", nil, "~ ~ ~")', 16, 3, "the cow summoned at the feet"),
    (OVERLAY, "hotbar.7 with minecraft:wheat", "hotbar.6 with minecraft:wheat", 16, 3, "the wheat in slot 7"),
    (OVERLAY, "hotbar.8 with minecraft:dandelion", "hotbar.7 with minecraft:dandelion", 16, 3, "the dandelion in slot 8"),
    (OVERLAY, "@e[type=minecraft:wolf,tag=sp7,limit=1,sort=nearest]", "@e[type=minecraft:wolf,tag=sp7,limit=1]", 16, 3, "any wolf owned"),
    (OVERLAY, "resistance 300 4 true", "resistance 300 3 true", 16, 3, "Resistance short of its highest"),
    (OVERLAY, 'mob("^ ^ ^7", "husk"', 'mob("^ ^ ^8", "husk"', 16, 3, "the husk on the sign's place"),
    (OVERLAY, '"bee", nil, "~ ~1 ~"', '"bee", nil, "~ ~0.5 ~"', 16, 3, "the bee half a block up"),
    (OVERLAY, 'label = "Pig##paths"', 'label = "Pig"', 16, 3, "the Pig's label the Mobs row's"),
    (OVERLAY, 'mob("^2 ^ ^2", "wolf"', 'mob("^-2 ^ ^2", "wolf"', 16, 3, "the wolf on the right"),
    (OVERLAY, "math.deg(atan2(dx, dy))", "math.deg(atan2(dy, dx))", 16, 4, "the heading from east"),
    (OVERLAY, 'dz < 0 and "down" or "up"', 'dz > 0 and "down" or "up"', 16, 4, "up and down swapped"),
    (OVERLAY, "math.sqrt(dx * dx + dy * dy)", "math.sqrt(dx * dx + dz * dz)", 16, 4, "the distance across the height"),
    (OVERLAY, "x = -1385.5, y = 1271.5", "x = -1385.5, y = -1271.5", 16, 4, "V's room at Minecraft's z"),
    (OVERLAY, 'ImGui.Text("    " .. line)', "ImGui.Text(line)", 16, 4, "the spots' lines not under their heading"),
    (SCRIPT, "look 270 0", "looks 270 0", 17, 2, "an action mclink doesn't know"),
    (SCRIPT, "type /item replace entity @s hotbar.8 with minecraft:dandelion", "type /item replace entity @s hotbar.9 with minecraft:dandelion", 17, 3, "the script's dandelion in slot 10"),
    (SCRIPT, "type /gamemode survival\n", "", 17, 3, "the script without Survival"),
]


def run_step(step):
    script = re.search(r"```bash\n(.*?)```", step.body, re.S).group(1).replace("/d/Cyberpunk-Minecraft", clone.as_posix())
    result = subprocess.run([shutil.which("bash"), "-c", script], cwd=clone, capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    out = result.stdout + result.stderr
    expected = re.findall(r"`([^`]+)`", step.body.split("Expected", 1)[1].split("\n\n", 1)[0])
    missing = [text for text in expected if text not in out]
    return expected, missing, out


for key in ((16, 3), (16, 4), (17, 2), (17, 3)):
    expected, missing, out = run_step(steps[key])
    if not expected or missing:
        raise SystemExit(f"unmutated, Task {key[0]} step {key[1]} fails: {missing}\n{out[-1500:]}")
print("unmutated: all four steps pass")
caught = 0
for path, old, new, task, number, what in MUTATIONS:
    target = clone / path
    text = target.read_bytes().decode("utf-8").replace("\r\n", "\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: the old text occurs {text.count(old)} times in {path}")
    target.write_bytes(text.replace(old, new).encode("utf-8"))
    try:
        _, missing, _ = run_step(steps[(task, number)])
    finally:
        subprocess.run(["git", "checkout", "--", path], cwd=clone, check=True)
    caught += bool(missing)
    print(f"{'caught' if missing else 'NOT CAUGHT'}: Task {task} step {number}, {what}" + (f" ({len(missing)} missing)" if missing else ""))
print(f"{caught} of {len(MUTATIONS)} mutations caught")
