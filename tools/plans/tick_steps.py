"""Tick some steps of one task in the plan: "- [ ] **Step N:" becomes "- [x] **Step N:" for the steps given.

    uv run --no-project python tick_steps.py <plan.md> <task> <step>...
"""
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
task = int(sys.argv[2])
steps = [int(s) for s in sys.argv[3:]]
raw = path.read_bytes().decode("utf-8")
newline = "\r\n" if "\r\n" in raw else "\n"
text = raw.replace("\r\n", "\n")
tasks = list(re.finditer(r"^### Task (\d+):", text, re.M))
index = next(i for i, m in enumerate(tasks) if int(m.group(1)) == task)
start = tasks[index].end()
end = tasks[index + 1].start() if index + 1 < len(tasks) else len(text)
section = text[start:end]
for step in steps:
    old = f"- [ ] **Step {step}:"
    if section.count(old) != 1:
        sys.exit(f"Task {task}: '{old}' occurs {section.count(old)} times")
    section = section.replace(old, f"- [x] **Step {step}:")
text = text[:start] + section + text[end:]
path.write_bytes(text.replace("\n", newline).encode("utf-8"))
print(f"Task {task}: ticked steps {steps}")
