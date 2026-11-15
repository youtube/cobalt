#!/usr/bin/env bash
# Applies the Cobalt 27.lts renderer-side adblock patch to a youtube/cobalt
# checkout.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH="$SCRIPT_DIR/patches/0001-cobalt-adblock-27lts.patch"

if [[ $# -gt 1 ]]; then
  echo "usage: $0 [path-to-cobalt-checkout]" >&2
  exit 2
fi

COBALT_DIR="${1:-${COBALT_DIR:-$PWD}}"

if [[ ! -f "$COBALT_DIR/cobalt/renderer/cobalt_content_renderer_client.cc" ]]; then
  echo "error: $COBALT_DIR does not look like a youtube/cobalt checkout" >&2
  echo "usage: $0 /path/to/cobalt" >&2
  exit 2
fi

if ! git -C "$COBALT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "error: $COBALT_DIR is not a git work tree" >&2
  exit 2
fi

if git -C "$COBALT_DIR" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
  echo "patch is already applied in $COBALT_DIR"
  exit 0
fi

if ! git -C "$COBALT_DIR" apply --check "$PATCH"; then
  echo "error: patch does not apply cleanly." >&2
  echo "check out 27.lts, 27.lts.10, or 27.lts.13 and retry." >&2
  exit 1
fi

git -C "$COBALT_DIR" apply "$PATCH"

echo "Applied $(basename "$PATCH") to $COBALT_DIR"
echo
echo "Next steps (32-bit ARM):"
echo "  $SCRIPT_DIR/build.sh $COBALT_DIR"
echo
echo "Or manually:"
echo "  cd $COBALT_DIR"
echo "  python3 cobalt/build/gn.py -p android-arm -c devel --no-rbe"
echo "  autoninja -C out/android-arm_devel cobalt_apk"
