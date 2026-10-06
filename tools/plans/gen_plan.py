"""Build a plan from its skeleton: every line "@@FILE <path>@@" becomes a fenced code block holding that
file, exactly.

    uv run --no-project python gen_plan.py <skeleton.md>... <out.md>

Paths are relative to spikes/block-render/, except extern/... and docs/..., which are relative to the repo's root.

Files come from the verified scratch clone, unless a line "@@SOURCE <commit>@@" says otherwise: from there on, files
come from that commit of the real repo, until the next such line ("@@SOURCE verify@@" goes back to the clone). A
task that ran long ago must show its files as they were when it ran, not as later tasks left them.

A line "@@EDIT <path> <from> <to>@@" becomes an Edit step's code blocks: the (old, new) pairs that turn the file at
<from> into the file at <to> (pairs.py). Each of <from> and <to> is "verify", a commit, or "snap:<name>": a snapshot
of files between two tasks, in snap/<name>/, laid out as the repo.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from pairs import make_pairs  # noqa: E402

CLONE = Path(os.environ.get("GEN_CLONE", Path(__file__).parent.parent / "verify"))
SNAPSHOTS = Path(__file__).parent / "snap8"
REPO = CLONE  # spike 6s Part 2 review: the tasks are tagged p2r-t23 to p2r-t30 in the verified clone
LANGUAGES = {".h": "cpp", ".cpp": "cpp", ".c": "c", ".hlsl": "hlsl", ".lua": "lua", ".py": "python", ".json": "json",
             ".md": "markdown", ".txt": "cmake", ".reds": "swift"}


def repo_path(path):
    return path if path.startswith(("extern/", "docs/")) else f"spikes/block-render/{path}"


def read(path, source):
    if source == "verify":
        return (CLONE / repo_path(path)).read_bytes()
    if source.startswith("snap:"):
        return (SNAPSHOTS / source[5:] / repo_path(path)).read_bytes()
    result = subprocess.run(["git", "-C", str(REPO), "show", f"{source}:{repo_path(path)}"], capture_output=True)
    if result.returncode != 0:
        sys.exit(f"{path} at {source}: {result.stderr.decode('utf-8', 'replace').strip()}")
    return result.stdout


def text_of(path, source):
    text = read(path, source).decode("utf-8").replace("\r\n", "\n")
    if not text.endswith("\n"):
        sys.exit(f"{path} at {source} doesn't end with a newline")
    return text


def fenced(path, text):
    longest = max((len(run) for run in re.findall(r"`+", text)), default=0)
    fence = "`" * max(3, longest + 1)
    name = Path(path)
    language = "cmake" if name.name == "CMakeLists.txt" else "text" if name.suffix == ".txt" else LANGUAGES[name.suffix]
    return f"{fence}{language}\n{text}{fence}\n"


def block(path, source):
    return fenced(path, text_of(path, source))


def edit_blocks(path, before, after):
    pairs = make_pairs(text_of(path, before), text_of(path, after))
    if not pairs:
        sys.exit(f"{path}: nothing changes from {before} to {after}")
    return "".join(fenced(path, old) + fenced(path, new) for old, new in pairs)


def expand(skeleton):
    out = []
    source = "verify"
    for line in skeleton.splitlines(keepends=True):
        directive = re.fullmatch(r"@@SOURCE (\S+)@@\n", line)
        placeholder = re.fullmatch(r"@@FILE (\S+)@@\n", line)
        edit = re.fullmatch(r"@@EDIT (\S+) (\S+) (\S+)@@\n", line)
        if directive:
            source = directive.group(1)
        elif placeholder:
            out.append(block(placeholder.group(1), source))
        elif edit:
            out.append(edit_blocks(*edit.groups()))
        else:
            out.append(line)
    return "".join(out)


parts = [Path(name).read_text(encoding="utf-8").replace("\r\n", "\n") for name in sys.argv[1:-1]]
out = expand("".join(parts))
left = re.findall(r"@@\w+", out)
if left:
    sys.exit(f"unexpanded placeholders: {left}")
Path(sys.argv[-1]).write_bytes(out.encode("utf-8"))
print(f"{sys.argv[-1]}: {out.count(chr(10))} lines")
