#!/usr/bin/env bash
# Replays Tasks 30 to 35 of Part 2's plan again, after a fold changed them: in the replay clone, from its Task 29 commit,
# which the full replay already matched with the verified clone's t29. The build folder goes first, so nothing built
# from later tasks' headers is reused. Then each task's commit is checked against the verified clone's tags, fetched
# again since the fold moved them.
# Usage: replay_from.sh <plan.md> <the replay clone's Task 29 commit>
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PLAN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
BASE="$2"
R="$HERE/replay6"
git -C "$R" checkout -q replay
git -C "$R" reset -q --hard "$BASE"
git -C "$R" clean -q -fdx
git -C "$R" fetch -q -f "$HERE/verify2" 'refs/tags/t*:refs/tags/v/t*'
if ! git -C "$R" diff --quiet "$BASE" v/t29 -- spikes docs/spikes; then
	echo "the Task 29 commit differs from the verified clone's t29"
	exit 1
fi
echo "from $(git -C "$R" log --oneline -1 "$BASE"), the same files as the verified clone's t29"
for range in "30 32" "34 35"; do
	set -- $range
	(cd "$HERE" && PYTHONIOENCODING=utf-8 uv run --no-project -q python run_plan.py replay "$PLAN" "$R" "$1" "$2")
done
tasks=(30 31 32 34 35)
commits=($(git -C "$R" rev-list --reverse "$BASE..replay"))
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
