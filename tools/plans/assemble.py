"""Assembles the plan with Part 3: the repo's plan, Task 37's step 2 ticked and its step 3's message as the handoff says,
then part3.md (gen_plan.py's output) at the end.

    uv run --no-project python assemble.py <repo plan> <part3.md> <out.md>
"""
import sys
from pathlib import Path

plan = Path(sys.argv[1]).read_bytes().decode("utf-8").replace("\r\n", "\n")
part3 = Path(sys.argv[2]).read_bytes().decode("utf-8").replace("\r\n", "\n")

edits = [
    (
        "- [ ] **Step 2: Plan Part 3**\n\n"
        "Use superpowers:writing-plans to add Part 3's tasks, from Task 38 on, to this plan: V against blocks (the spec's\n"
        "\"Part 3\" and step 4), with the audit of the calls that make bodies first. Verify them as Parts 1 and 2 were. Changes to\n"
        "the spec found while planning go in its \"Changes while planning\".\n",
        "- [x] **Step 2: Plan Part 3**\n\n"
        "Use superpowers:writing-plans to add Part 3's tasks, from Task 38 on, to this plan: Part 2's two follow-ups first, as\n"
        "the user asked, then V against blocks (the spec's \"Part 3\" and step 4), with the audit of the calls that make bodies\n"
        "before step 4. Verify them as Parts 1 and 2 were. Changes to the spec found while planning go in its \"Changes while\n"
        "planning\".\n",
    ),
    (
        '  && git commit -q -m "collision-and-movement plan: Part 2 closed, Part 3 planned" \\\n',
        '  && git commit -q -m "collision-and-movement plan: Part 3 planned" \\\n',
    ),
]
for old, new in edits:
    count = plan.count(old)
    if count != 1:
        sys.exit(f"{count} matches for:\n{old}")
    plan = plan.replace(old, new)
if not plan.endswith("\n") or not part3.startswith("\n---\n"):
    sys.exit("the parts don't join as expected")
out = plan + part3
Path(sys.argv[3]).write_bytes(out.encode("utf-8"))
print(f"{sys.argv[3]}: {len(out.splitlines())} lines")
