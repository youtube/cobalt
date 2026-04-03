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

# This script builds and deploys Cobalt Evergreen components to an RDK device via ADB.
# It supports deploying the full executable environment or just the compressed library.

set -e

# Handle arguments
RDK_DEVICE_ID="$1"
COMMAND="${2:-all}"

if [ -z "${RDK_DEVICE_ID}" ]; then
  echo "Usage: $0 <device_id> [all|lib]"
  echo ""
  echo "Commands:"
  echo "  all  : (Default) Build and deploy everything (loader_app, content, and libcobalt.lz4)."
  echo "  lib  : Build and deploy ONLY the compressed libcobalt.lz4."
  echo ""
  echo "Error: Device ID is required (obtain via 'adb devices')."
  exit 1
fi

# Automatically determine the repository root
REPO_DIR=$(git rev-parse --show-toplevel 2>/dev/null || realpath "$(dirname "$0")/../../../../../../../../")

if [ ! -d "${REPO_DIR}/cobalt" ]; then
  echo "Error: Could not determine repository root. Please run this script from within the Cobalt source tree."
  exit 1
fi

cd "${REPO_DIR}"
echo "=== Working in repository: ${REPO_DIR} ==="

PLATFORM="evergreen-arm-hardfp-rdk"
CONFIG="qa"
OUT_DIR="out/${PLATFORM}_${CONFIG}"
REMOTE_DIR="/data/out_cobalt_exe" # Avoid using /data/out_cobalt, as it is reserved for plugins
ARCHIVE_NAME="archive.tar.gz"

# For 'lib' only mode, we just build and push the compressed library.
if [ "${COMMAND}" == "lib" ]; then
    if [ ! -d "${OUT_DIR}" ]; then
        echo "=== Output directory not found. Configuring first... ==="
        python3 cobalt/build/gn.py -p "${PLATFORM}" -C "${CONFIG}"
    fi

    echo "=== Building Cobalt Library (libcobalt.lz4) ==="
    autoninja -C "${OUT_DIR}" copy_cobalt_lib
    
    echo "=== Deploying libcobalt.lz4 to ${RDK_DEVICE_ID} ==="
    adb -s "${RDK_DEVICE_ID}" shell "mkdir -p ${REMOTE_DIR}/app/cobalt/lib"
    adb -s "${RDK_DEVICE_ID}" push "${OUT_DIR}/app/cobalt/lib/libcobalt.lz4" "${REMOTE_DIR}/app/cobalt/lib/"
    echo "=== Finished (Library Only) ==="
    exit 0
fi

echo "=== Configuring Cobalt for ${PLATFORM} (${CONFIG}) ==="
python3 cobalt/build/gn.py -p "${PLATFORM}" -C "${CONFIG}"

# Define build targets
BUILD_TARGETS="loader_app cobalt_loader copy_cobalt_lib"

echo "=== Building Targets: ${BUILD_TARGETS} ==="
autoninja -C "${OUT_DIR}" ${BUILD_TARGETS}

echo "=== Packaging artifacts ==="
tar -czvf "${ARCHIVE_NAME}" -C "${OUT_DIR}" -T "${OUT_DIR}/cobalt_loader.runtime_deps" gen/build_info.json

echo "=== Deploying to device (${RDK_DEVICE_ID}) via ADB ==="
adb -s "${RDK_DEVICE_ID}" shell "mkdir -p ${REMOTE_DIR}"
adb -s "${RDK_DEVICE_ID}" push "${ARCHIVE_NAME}" "${REMOTE_DIR}/" && rm "${ARCHIVE_NAME}"

# Use a login shell (bash -l) because standard adb shell doesn't source /etc/profile,
# which is required to set up critical environment variables like WAYLAND_DISPLAY,
# LD_PRELOAD, and XDG_RUNTIME_DIR for display and graphics support.
echo "=== Extracting and running on device via ADB ==="
adb -s "${RDK_DEVICE_ID}" shell "bash -l -c 'cd ${REMOTE_DIR} && tar -xzvf ${ARCHIVE_NAME} && rm ${ARCHIVE_NAME} && rdkDisplay remove || true && sleep 2 && rdkDisplay create && sleep 2 && ./loader_app && rdkDisplay remove'"

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
