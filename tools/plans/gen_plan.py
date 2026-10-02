"""Build the block-render plan from its skeleton: every line "@@FILE <path>@@" becomes a fenced code block holding that
file, exactly.

    uv run --no-project python gen_plan.py <skeleton.md>... <out.md>

Paths are relative to spikes/block-render/, except extern/... and docs/..., which are relative to the repo's root.

Files come from the verified scratch clone, unless a line "@@SOURCE <commit>@@" says otherwise: from there on, files
come from that commit of the real repo, until the next such line ("@@SOURCE verify@@" goes back to the clone). A
task that ran long ago must show its files as they were when it ran, not as later tasks left them.
"""
import re
import subprocess
import sys
from pathlib import Path

CLONE = Path(__file__).parent / "verify"
REPO = Path("D:/Cyberpunk-Minecraft")
LANGUAGES = {".h": "cpp", ".cpp": "cpp", ".c": "c", ".hlsl": "hlsl", ".lua": "lua", ".py": "python", ".json": "json",
             ".md": "markdown", ".txt": "cmake"}


def repo_path(path):
    return path if path.startswith(("extern/", "docs/")) else f"spikes/block-render/{path}"


def read(path, source):
    if source == "verify":
        return (CLONE / repo_path(path)).read_bytes()
    result = subprocess.run(["git", "-C", str(REPO), "show", f"{source}:{repo_path(path)}"], capture_output=True)
    if result.returncode != 0:
        sys.exit(f"{path} at {source}: {result.stderr.decode('utf-8', 'replace').strip()}")
    return result.stdout


def block(path, source):
    text = read(path, source).decode("utf-8").replace("\r\n", "\n")
    if not text.endswith("\n"):
        sys.exit(f"{path} at {source} doesn't end with a newline")
    longest = max((len(run) for run in re.findall(r"`+", text)), default=0)
    fence = "`" * max(3, longest + 1)
    return f"{fence}{LANGUAGES[Path(path).suffix]}\n{text}{fence}\n"


def expand(skeleton):
    out = []
    source = "verify"
    for line in skeleton.splitlines(keepends=True):
        directive = re.fullmatch(r"@@SOURCE (\S+)@@\n", line)
        placeholder = re.fullmatch(r"@@FILE (\S+)@@\n", line)
        if directive:
            source = directive.group(1)
        elif placeholder:
            out.append(block(placeholder.group(1), source))
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
