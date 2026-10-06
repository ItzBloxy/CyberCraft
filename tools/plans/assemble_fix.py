"""Puts step 5b's fix into the plan: fix.md (gen_plan.py's output, Tasks 49 to 52) after Task 48, Task 49's close renumbered
53, Part 3's introduction, file map, the header and "Before you start" brought up to the fix, and "Step 5b's fix" at the end.

    uv run --no-project python assemble_fix.py <repo plan> <fix.md> <fixend.md> <out.md>
"""
import sys
from pathlib import Path

plan = Path(sys.argv[1]).read_bytes().decode("utf-8").replace("\r\n", "\n")
fix = Path(sys.argv[2]).read_bytes().decode("utf-8").replace("\r\n", "\n")
end = Path(sys.argv[3]).read_bytes().decode("utf-8").replace("\r\n", "\n")

edits = [
    (
        "**Part 3** (Tasks 38 to 49) makes Minecraft's blasts knock down and throw NPCs, and push cars and props, through the\n"
        "game's own explosion code, with the game's knockdown grenade's attack; and takes the follow-ups Part 2 left:\n"
        "Minecraft's health from V's as build mode starts, bushes out of Minecraft's ground, and the blades' heat after build\n"
        "mode. It was planned after step 4's fixes, from Task 37's handoff\n"
        "(`docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md`), and its own section below says how it's built.\n",
        "**Part 3** (Tasks 38 to 53) makes Minecraft's blasts knock down and throw NPCs, and push cars and props, through the\n"
        "game's own explosion code, with the game's knockdown grenade's attack; and takes the follow-ups Part 2 left:\n"
        "Minecraft's health from V's as build mode starts, bushes out of Minecraft's ground, and the blades' heat after build\n"
        "mode. It was planned after step 4's fixes, from Task 37's handoff\n"
        "(`docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md`), and its own section below says how it's built.\n"
        "Step 5b (Task 48) passed but for living NPCs' knockdowns, which the knockdown's status effect alone doesn't make on a\n"
        "crowd NPC: Tasks 49 to 51 knock them down with the game's forced reaction instead, and tell the crowd with the game's\n"
        "explosion stimulus, and Task 52 checks it in the game; \"Step 5b's fix\", at the end, says why.\n",
    ),
    (
        "  without and with Codeware's, and a slip in `Push` fails it both ways. CyberCraft Tweaks 1.3.0 builds without a\n"
        "  warning. The overlay ran under Lua with CET's functions stubbed, with a blast before and after its push, and combat\n"
        "  off.\n",
        "  without and with Codeware's, and a slip in `Push` fails it both ways. CyberCraft Tweaks 1.3.0 builds without a\n"
        "  warning. The overlay ran under Lua with CET's functions stubbed, with a blast before and after its push, and combat\n"
        "  off.\n"
        "- **Step 5b's fix, verified the same way:** Tasks 49 to 51 were written in the scratch clone on top of Task 47's\n"
        "  commit, then replayed in a fresh clone from this plan's text: zero warnings, 450 tests, the files identical to the\n"
        "  scratch clone's. The changed test was broken once per change: 3 mutations, all caught. The module compiles against\n"
        "  the game's own scripts both without and with Codeware's, and a slip in `Explode` fails it both ways.\n",
    ),
    (
        "  42, and blasts in the game in Task 48. All with the user.\n",
        "  42, and blasts in the game in Tasks 48 and 52. All with the user.\n",
    ),
    (
        "- **The game:** builds deploy only with `CP2077_DIR` set, which only the deploy steps set (Tasks 11, 31, 36 and 48),\n",
        "- **The game:** builds deploy only with `CP2077_DIR` set, which only the deploy steps set (Tasks 11, 31, 36, 48 and 52),\n",
    ),
    (
        "- **Task 48** is step 5b, in the game. **Task 49** closes the spike.\n",
        "- **Task 48** is step 5b, in the game. It passed but for living NPCs' knockdowns, and found that the crowd doesn't\n"
        "  panic: **Tasks 49 to 51** make step 5b's fix, and **Task 52** checks it in the game.\n"
        "- **Task 53** closes the spike.\n",
    ),
    (
        "Task 45. Tasks 41, 43, 44, 46 and 47 add none.\n",
        "Task 45. Tasks 41, 43, 44, 46 and 47 add none, nor do step 5b's fix's Tasks 49 to 51: Task 49 changes a test.\n",
    ),
    (
        "| `scripts/BlockRender.reds` | Version 6: `Explode`, `Push` and `BlastHit` (with `Hit`, through `Strike`); V's health share; the blades drawn again after build mode |\n",
        "| `scripts/BlockRender.reds` | Version 6: `Explode`, `Push` and `BlastHit` (with `Hit`, through `Strike`); V's health share; the blades drawn again after build mode. Version 7: `Explode` knocks living NPCs down with the game's forced reaction and tells the crowd |\n",
    ),
    (
        "| `src/GameScript.*` | The module's three new calls, and V's health share and what `Push` found |\n",
        "| `src/GameScript.*` | The module's three new calls, and V's health share and what `Push` found; then what `Explode` did |\n",
    ),
    (
        "| `src/Requests.*`, `src/Status.*` | `explode`; the blasts line, Minecraft's health in the combat line, the bush rule in the ground line |\n",
        "| `src/Requests.*`, `src/Status.*` | `explode`; the blasts line, Minecraft's health in the combat line, the bush rule in the ground line; then whether the forced knockdowns are off |\n",
    ),
    (
        "| `src/Plugin.cpp`, `CMakeLists.txt` | 0.19.0: blasts, their hits and pushes in `Fight`, V's health share as build mode starts, `explode`, `set blastpower`, `set blastknock` and `set bushrule` |\n",
        "| `src/Plugin.cpp`, `CMakeLists.txt` | 0.19.0: blasts, their hits and pushes in `Fight`, V's health share as build mode starts, `explode`, `set blastpower`, `set blastknock` and `set bushrule`. 0.19.1: the forced knockdowns on by default, and what each blast did in the log |\n",
    ),
    ("### Task 49: Close the spike\n", fix + "### Task 53: Close the spike\n"),
]
for old, new in edits:
    count = plan.count(old)
    if count != 1:
        sys.exit(f"{count} matches for:\n{old}")
    plan = plan.replace(old, new)
if not plan.endswith("\n") or not end.startswith("\n## Step 5b's fix\n"):
    sys.exit("the end doesn't join as expected")
out = plan + end
Path(sys.argv[4]).write_bytes(out.encode("utf-8"))
print(f"{sys.argv[4]}: {len(out.splitlines())} lines")
