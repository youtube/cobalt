#!/bin/bash
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script builds and deploys the standalone Cobalt Evergreen executable (loader_app)
# to an RDK device via ADB. It does not handle Cobalt plugins.

set -e

# Handle Device ID from argument
RDK_DEVICE_ID="$1"

if [[ -z "${RDK_DEVICE_ID}" ]]; then
  echo "Usage: $0 <device_id>"
  echo "Error: Device ID is required (obtain via 'adb devices')." >&2
  exit 1
fi

# Automatically determine the repository root
# Correct depth for starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/ is 9 levels up.
REPO_DIR=$(git rev-parse --show-toplevel 2>/dev/null || realpath "$(dirname "$0")/../../../../../../../../../")

if [[ ! -d "${REPO_DIR}/cobalt" ]]; then
  echo "Error: Could not determine repository root. Please run this script from within the Cobalt source tree." >&2
  exit 1
fi

cd "${REPO_DIR}"
echo "=== Working in repository: ${REPO_DIR} ==="

PLATFORM="evergreen-arm-hardfp-rdk"
CONFIG="qa"
OUT_DIR="out/${PLATFORM}_${CONFIG}"
REMOTE_DIR="/data/out_cobalt_exe" # Avoid using /data/out_cobalt, as it is reserved for plugins
ARCHIVE_NAME="archive.tar.gz"

echo "=== Configuring Cobalt for ${PLATFORM} (${CONFIG}) ==="
python3 cobalt/build/gn.py -p "${PLATFORM}" -C "${CONFIG}"

echo "=== Building Cobalt Executable (loader_app) ==="
autoninja -C "${OUT_DIR}" loader_app cobalt_loader

echo "=== Packaging artifacts ==="
# Stage build_info.json inside OUT_DIR to ensure tar can find it with -C
mkdir -p "${OUT_DIR}/gen"
cp "gen/build_info.json" "${OUT_DIR}/gen/"
tar -czvf "${ARCHIVE_NAME}" -C "${OUT_DIR}" -T "${OUT_DIR}/cobalt_loader.runtime_deps" "gen/build_info.json"

echo "=== Deploying to device (${RDK_DEVICE_ID}) via ADB ==="
adb -s "${RDK_DEVICE_ID}" shell "mkdir -p ${REMOTE_DIR}"
adb -s "${RDK_DEVICE_ID}" push "${ARCHIVE_NAME}" "${REMOTE_DIR}/" && rm "${ARCHIVE_NAME}"

# Use a login shell (bash -l) because standard adb shell doesn't source /etc/profile,
# which is required to set up critical environment variables like WAYLAND_DISPLAY,
# LD_PRELOAD, and XDG_RUNTIME_DIR for display and graphics support.
echo "=== Extracting and running on device via ADB ==="
REMOTE_COMMANDS="cd ${REMOTE_DIR} && \
  tar -xzvf ${ARCHIVE_NAME} && \
  rm ${ARCHIVE_NAME} && \
  rdkDisplay remove || true && \
  sleep 2 && \
  rdkDisplay create && \
  sleep 2 && \
  ./loader_app && \
  rdkDisplay remove"

adb -s "${RDK_DEVICE_ID}" shell "bash -l -c '${REMOTE_COMMANDS}'"

echo ""
echo "------------------------------------------------------------------------------------------------"
echo "NOTE: The screen will be non-interactive with the remote until 'rdkDisplay create' is run"
echo "(handled automatically by this script)."
echo ""
echo "RECOMMENDED: To stop Cobalt gracefully, use the back button on remote or esc key on keyboard."
echo "This ensures the loader_app process terminates correctly and rdkDisplay remove is called,"
echo "so that you can control the home screen with your remote."
echo "------------------------------------------------------------------------------------------------"
echo ""

echo "=== Finished ==="
