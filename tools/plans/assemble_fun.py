"""Puts the Fun controls into the plan: fun.md (gen_plan.py's output, Task 53) before the close, renumbered 54; the header,
"Before you start", Part 3's introduction and file map, and Task 52's record brought up to it; "The Fun controls" at the end.

    uv run --no-project python assemble_fun.py <repo plan> <fun.md> <funend.md> <out.md>
"""
import sys
from pathlib import Path

plan = Path(sys.argv[1]).read_bytes().decode("utf-8").replace("\r\n", "\n")
fun = Path(sys.argv[2]).read_bytes().decode("utf-8").replace("\r\n", "\n")
end = Path(sys.argv[3]).read_bytes().decode("utf-8").replace("\r\n", "\n")

edits = [
    (
        "**Part 3** (Tasks 38 to 53) makes Minecraft's blasts knock down and throw NPCs, and push cars and props, through the\n",
        "**Part 3** (Tasks 38 to 54) makes Minecraft's blasts knock down and throw NPCs, and push cars and props, through the\n",
    ),
    (
        "explosion stimulus, and Task 52 checks it in the game; \"Step 5b's fix\", at the end, says why.\n",
        "explosion stimulus, and Task 52 checks it in the game; \"Step 5b's fix\", at the end, says why. During Task 52 the user\n"
        "asked for the overlay's Fun controls, which Task 53 makes and installs with the game running; \"The Fun controls\",\n"
        "at the end, says why.\n",
    ),
    (
        "  the game's own scripts both without and with Codeware's, and a slip in `Explode` fails it both ways.\n",
        "  the game's own scripts both without and with Codeware's, and a slip in `Explode` fails it both ways.\n"
        "- **The Fun controls, verified the same way:** Task 53 was written in the scratch clone on top of Task 51's commit: the\n"
        "  overlay's syntax checked, and the overlay run under Lua with CET's functions stubbed (`tools/plans/fun_check.lua`),\n"
        "  every Fun button clicked and each request checked, with the request file read, and left unread. Its file step,\n"
        "  applied from this plan's text to a clone of Task 51's commit, gave the file identical to the scratch clone's.\n",
    ),
    (
        "  42, and blasts in the game in Tasks 48 and 52. All with the user.\n",
        "  42, blasts in the game in Tasks 48 and 52, and the Fun controls in Task 53. All with the user.\n",
    ),
    (
        "  with the game closed, and clear straight after. `mclink` needs the game closed too, since the game would hold the\n"
        "  mapping.\n",
        "  with the game closed, and clear straight after. `mclink` needs the game closed too, since the game would hold the\n"
        "  mapping. Task 53 alone installs with the game running, as the user chose: the overlay's file, copied, which nothing\n"
        "  holds.\n",
    ),
    (
        "- **Task 53** closes the spike.\n",
        "- **Task 53** makes the overlay's Fun controls, asked for during Task 52. **Task 54** closes the spike.\n",
    ),
    (
        "| `tools/blockrender_hud/init.lua` | The blasts line, and \"TNT 5 m ahead\" |\n",
        "| `tools/blockrender_hud/init.lua` | The blasts line, and \"TNT 5 m ahead\"; then the Fun controls, and a queue for the requests |\n",
    ),
    (
        "and what the user saw; `set blastknock 0`; the crowd, and any police; the TNT; other NPCs, or that none were about; and\n"
        "anything that surprised. Update the spike's answer at the top.\n",
        "and what the user saw; `set blastknock 0`; the crowd, and any police; the TNT; other NPCs, or that none were about; the\n"
        "Fun controls, from Task 53; and anything that surprised. Update the spike's answer at the top, and add Minecraft's\n"
        "inventory and its game mode switcher under \"Asked for, after Part 3\" (the spec's \"Asked for during step 5b's fix\").\n",
    ),
    ("### Task 53: Close the spike\n", fun + "### Task 54: Close the spike\n"),
    (
        "and throw NPCs and push cars and props, and what is left. At the end of this plan, after \"Step 5b's fix\":\n",
        "and throw NPCs and push cars and props, and what is left. At the end of this plan, after \"The Fun controls\":\n",
    ),
]
for old, new in edits:
    count = plan.count(old)
    if count != 1:
        sys.exit(f"{count} matches for:\n{old}")
    plan = plan.replace(old, new)
if not plan.endswith("\n") or not end.startswith("\n## The Fun controls\n"):
    sys.exit("the end doesn't join as expected")
out = plan + end
Path(sys.argv[4]).write_bytes(out.encode("utf-8"))
print(f"{sys.argv[4]}: {len(out.splitlines())} lines")
