#!/usr/bin/env bash
# Configures and builds the Cobalt APK for 32-bit ARM (android-arm).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -gt 1 ]]; then
  echo "usage: $0 [path-to-cobalt-checkout]" >&2
  exit 2
fi

COBALT_DIR="${1:-${COBALT_DIR:-$PWD}}"
PLATFORM="${PLATFORM:-android-arm}"
BUILD_TYPE="${BUILD_TYPE:-devel}"

if [[ ! -f "$COBALT_DIR/cobalt/build/gn.py" ]]; then
  echo "error: $COBALT_DIR does not look like a youtube/cobalt checkout" >&2
  echo "usage: $0 /path/to/cobalt" >&2
  exit 2
fi

if [[ ! -d "$COBALT_DIR/cobalt/adblock" ]]; then
  echo "error: cobalt/adblock is missing; run apply.sh first" >&2
  exit 1
fi

for tool in gn autoninja; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: $tool not found; add depot_tools to PATH" >&2
    exit 1
  fi
done

cd "$COBALT_DIR"

GN_ARGS=()
if [[ "${USE_RBE:-0}" != "1" ]]; then
  GN_ARGS+=(--no-rbe)
fi

python3 cobalt/build/gn.py -p "$PLATFORM" -c "$BUILD_TYPE" "${GN_ARGS[@]}"
autoninja -C "out/${PLATFORM}_${BUILD_TYPE}" cobalt_apk

APK="$COBALT_DIR/out/${PLATFORM}_${BUILD_TYPE}/apks/Cobalt.apk"
echo
if [[ -f "$APK" ]]; then
  echo "APK: $APK"
else
  echo "Build finished; locate the APK with:"
  echo "  find out/${PLATFORM}_${BUILD_TYPE} -name '*.apk'"
fi
