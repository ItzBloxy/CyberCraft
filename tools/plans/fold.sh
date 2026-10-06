#!/usr/bin/env bash
# Folds fixup commits into the Part 3 tasks they belong to: rebuilds the verified clone's history from the tag before the
# first task named, cherry-picking each task's commit and then its fixups, then the commits on top that aren't fixups,
# checks the result is the same tree as before, and only then moves the branch and the tags. A task with no code (one
# with the user) has its tag on the same commit as the task before, and keeps it so.
# Usage: fold.sh <first task> <last task> <task tag>=<fixup commit> ...   (on the branch whose head has the fixups)
set -euo pipefail
cd "$(dirname "$0")/../verify"
first="$1"; last="$2"; shift 2
declare -A fix
for pair in "$@"; do
	fix["${pair%%=*}"]="${fix["${pair%%=*}"]:-} $(git rev-parse "${pair#*=}")"
done
before="$(git rev-parse HEAD)"
branch="$(git branch --show-current)"
# The commits after the last task's tag that aren't fixups, oldest first.
fixups=" ${fix[*]:-} "
rest=()
for commit in $(git rev-list --reverse "p3-t$last..HEAD"); do
	[[ "$fixups" == *" $commit "* ]] || rest+=("$commit")
done
git checkout -q --detach "p3-t$((first - 1))"
declare -A made
for ((n = first; n <= last; ++n)); do
	tag="p3-t$n"
	if [ "$n" -gt "$first" ] && [ "$(git rev-parse "$tag")" = "$(git rev-parse "p3-t$((n - 1))")" ]; then
		made[$tag]="${made[p3-t$((n - 1))]}"
		continue
	fi
	git cherry-pick "$tag" > /dev/null
	for commit in ${fix[$tag]:-}; do
		git cherry-pick -n "$commit"
		git commit -q --amend --no-edit
	done
	made[$tag]="$(git rev-parse HEAD)"
done
for commit in "${rest[@]}"; do
	git cherry-pick "$commit" > /dev/null
done
if [ -n "$(git diff "$before" HEAD)" ]; then
	echo "the rebuilt tree differs from $before: nothing moved"
	git checkout -q "$branch"
	exit 1
fi
for tag in "${!made[@]}"; do
	git tag -f "$tag" "${made[$tag]}" > /dev/null
done
git branch -f "$branch" HEAD
git checkout -q "$branch"
echo "folded; $branch is at $(git rev-parse --short HEAD), the same tree as $(git rev-parse --short "$before")"
