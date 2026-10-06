"""Assembles the plan with spike 6's Part 3: the repo's plan, its header and "Before you start" brought up to Part 3,
part3.md (gen_plan.py's output) put after Task 37 and before "Found while planning", and found3.md at the end.

    uv run --no-project python assemble3.py <repo plan> <part3.md> <found3.md> <out.md>
"""
import sys
from pathlib import Path

plan = Path(sys.argv[1]).read_bytes().decode("utf-8").replace("\r\n", "\n")
part3 = Path(sys.argv[2]).read_bytes().decode("utf-8").replace("\r\n", "\n")
found3 = Path(sys.argv[3]).read_bytes().decode("utf-8").replace("\r\n", "\n")

edits = [
    (
        "# NPCs and Combat Implementation Plan, Parts 1 and 2 (steps 1 to 4)\n",
        "# NPCs and Combat Implementation Plan, Parts 1 to 3 (steps 1 to 5)\n",
    ),
    (
        "**Part 3** is planned after step 4's fixes, as the spec says: Task 37 closes Part 2 and, at the user's choice, writes a\n"
        "handoff for planning Part 3 in a new session (`docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md`).\n",
        "**Part 3** (Tasks 38 to 49) makes Minecraft's blasts knock down and throw NPCs, and push cars and props, through the\n"
        "game's own explosion code, with the game's knockdown grenade's attack; and takes the follow-ups Part 2 left:\n"
        "Minecraft's health from V's as build mode starts, bushes out of Minecraft's ground, and the blades' heat after build\n"
        "mode. It was planned after step 4's fixes, from Task 37's handoff\n"
        "(`docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md`), and its own section below says how it's built.\n",
    ),
    (
        "  scratch clone's. Each new or changed test was broken once: 9 mutations, all caught. The module compiles against the\n"
        "  game's own scripts both without and with Codeware's, and a slip in its Codeware parts fails only with Codeware's.\n",
        "  scratch clone's. Each new or changed test was broken once: 9 mutations, all caught. The module compiles against the\n"
        "  game's own scripts both without and with Codeware's, and a slip in its Codeware parts fails only with Codeware's.\n"
        "- **Part 3, verified the same way:** Tasks 38 to 41 and 43 to 47 were written in a scratch clone, then replayed in a\n"
        "  fresh clone from this plan's text: zero warnings, 450 tests, the files identical to the scratch clone's. Each new or\n"
        "  changed test was broken once: 38 mutations, all caught. The module compiles against the game's own scripts both\n"
        "  without and with Codeware's, and a slip in `Push` fails it both ways. CyberCraft Tweaks 1.3.0 builds without a\n"
        "  warning. The overlay ran under Lua with CET's functions stubbed, with a blast before and after its push, and combat\n"
        "  off.\n",
    ),
    (
        "  and the NPCs in the game in Task 11; in Part 2, the hurts against SkyCraft's mod in Task 16, and combat in the game in\n"
        "  Tasks 31 and 36. All with the user.\n",
        "  and the NPCs in the game in Task 11; in Part 2, the hurts against SkyCraft's mod in Task 16, and combat in the game in\n"
        "  Tasks 31 and 36; in Part 3, a real blast's hits and the health against SkyCraft's mod and CyberCraft Tweaks in Task\n"
        "  42, and blasts in the game in Task 48. All with the user.\n",
    ),
    (
        "  - **approves downloads first.** Nothing in Parts 1 and 2 downloads anything.\n",
        "  - **approves downloads first.** Nothing in Parts 1 to 3 downloads anything;\n"
        "  - **approves changes to their Minecraft instance:** Task 42 installs CyberCraft Tweaks 1.3.0 in place of 1.2.0.\n",
    ),
    (
        "- **The game:** builds deploy only with `CP2077_DIR` set, which only the deploy steps set (Tasks 11, 31 and 36), with the\n",
        "- **The game:** builds deploy only with `CP2077_DIR` set, which only the deploy steps set (Tasks 11, 31, 36 and 48), with the\n",
    ),
]
for old, new in edits:
    count = plan.count(old)
    if count != 1:
        sys.exit(f"{count} matches for:\n{old}")
    plan = plan.replace(old, new)

anchor = "---\n\n## Found while planning\n"
if plan.count(anchor) != 1 or not part3.startswith("## Part 3") or not part3.endswith("---\n\n"):
    sys.exit("the parts don't join as expected")
plan = plan.replace(anchor, "---\n\n" + part3 + "## Found while planning\n")
if not plan.endswith("\n") or not found3.startswith("\n## Found while planning Part 3\n"):
    sys.exit("the end doesn't join as expected")
out = plan + found3
Path(sys.argv[4]).write_bytes(out.encode("utf-8"))
print(f"{sys.argv[4]}: {len(out.splitlines())} lines")
