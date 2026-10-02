"""Tick every step of one task in the plan: "- [ ] **Step" becomes "- [x] **Step" within that task.

    uv run --no-project python tick.py <plan.md> <task>
"""
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
task = int(sys.argv[2])
raw = path.read_bytes().decode("utf-8")
newline = "\r\n" if "\r\n" in raw else "\n"
text = raw.replace("\r\n", "\n")
tasks = list(re.finditer(r"^### Task (\d+):", text, re.M))
index = next(i for i, m in enumerate(tasks) if int(m.group(1)) == task)
start = tasks[index].end()
end = tasks[index + 1].start() if index + 1 < len(tasks) else len(text)
section = text[start:end]
ticked = section.replace("- [ ] **Step", "- [x] **Step")
count = section.count("- [ ] **Step")
text = text[:start] + ticked + text[end:]
path.write_bytes(text.replace("\n", newline).encode("utf-8"))
print(f"Task {task}: ticked {count} steps")
