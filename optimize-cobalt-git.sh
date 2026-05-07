#!/usr/bin/env bash
# Apply git performance optimizations to the Cobalt source repo.
# See docs/cobalt-git-optimizations.md for rationale.
#
# Run from the root of the RDK repository.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

COBALT_SRC="${COBALT_BUILD_DIR}/src"

if [[ ! -d "$COBALT_SRC/.git" ]]; then
    echo "Error: $COBALT_SRC is not a git repository." >&2
    exit 1
fi

echo "Applying git optimizations to $COBALT_SRC ..."

git -C "$COBALT_SRC" config core.fsmonitor       false
git -C "$COBALT_SRC" config core.checkStat       minimal
git -C "$COBALT_SRC" config core.untrackedcache  true
git -C "$COBALT_SRC" config index.version        4
git -C "$COBALT_SRC" config index.threads        0
git -C "$COBALT_SRC" config index.skipHash       true
git -C "$COBALT_SRC" config feature.manyFiles    true
git -C "$COBALT_SRC" config status.showUntrackedFiles no

echo "Rewriting index in v4 format (may take a moment) ..."
git -C "$COBALT_SRC" update-index --index-version=4

echo "Done. 'git status' should now run in ~0.5–0.9 s instead of ~2 s."
echo "Use 'git -C $COBALT_SRC status -u' when you need to see untracked files."
