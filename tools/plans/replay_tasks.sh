#!/usr/bin/env bash
# Replays Part 2's plan in a fresh clone of the real repo at the branch's head, task range by task range (the tasks with
# the user are left out), then checks each task's files against the verified clone's tag.
# Usage: replay_tasks.sh <plan.md>
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PLAN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
R="$HERE/replay6"
rm -rf "$R"
git clone -q /d/Cyberpunk-Minecraft "$R"
git -C "$R" checkout -q -b replay origin/spike/collision-and-movement
git -C "$R" fetch -q "$HERE/verify2" 'refs/tags/t*:refs/tags/v/t*'
for range in "19 27" "29 32" "34 35"; do
	set -- $range
	(cd "$HERE" && PYTHONIOENCODING=utf-8 uv run --no-project -q python run_plan.py replay "$PLAN" "$R" "$1" "$2")
done
# Each task's commit, oldest first, against the verified clone's tag for the same task.
tasks=(19 20 21 22 23 24 25 26 27 29 30 31 32 34 35)
commits=($(git -C "$R" rev-list --reverse "origin/spike/collision-and-movement..replay"))
if [ "${#commits[@]}" -ne "${#tasks[@]}" ]; then
	echo "${#commits[@]} commits for ${#tasks[@]} tasks"
	exit 1
fi
for i in "${!tasks[@]}"; do
	if git -C "$R" diff --quiet "${commits[$i]}" "v/t${tasks[$i]}" -- spikes docs/spikes; then
		echo "task ${tasks[$i]}: the same files as the verified clone's"
	else
		echo "task ${tasks[$i]}: DIFFERS from the verified clone's:"
		git -C "$R" diff --stat "${commits[$i]}" "v/t${tasks[$i]}" -- spikes docs/spikes
	fi
done
