#!/bin/bash
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

set -euo pipefail

readonly XVFB_DISPLAY=":393"

COBALT_PATH=""
CONFIG="qa"
DOWNLOAD_CHROMEDRIVER=1
OUTPUT_DIR="${HOME}/code/chromedriver_tests"

while [[ "$#" -gt 0 ]]; do
  case $1 in
    --cobalt-path) COBALT_PATH="$2"; shift ;;
    --config) CONFIG="$2"; shift ;;
    --output-dir) OUTPUT_DIR="$2"; shift ;;
    --download-chromedriver) DOWNLOAD_CHROMEDRIVER=1 ;;
    --no-download-chromedriver) DOWNLOAD_CHROMEDRIVER=0 ;;
    *) echo "Unknown parameter passed: $1"; exit 1 ;;
  esac
  shift
done

if [[ -z "${COBALT_PATH}" ]]; then
  echo "Usage: $0 --cobalt-path <path> [--config <config>] [--output-dir <dir>] [--download-chromedriver|--no-download-chromedriver]"
  exit 1
fi

OUT_DIR="${COBALT_PATH}/out/linux-x64x11_${CONFIG}"
COBALT_BIN="${OUT_DIR}/cobalt"
COBALT_STRIPPED_BIN="${OUT_DIR}/cobalt.stripped"
CHROMEDRIVER_BIN="${OUT_DIR}/chromedriver"

if [[ ! -f "${COBALT_BIN}" ]]; then
  echo "Error: Cobalt binary not found at ${COBALT_BIN}"
  exit 1
fi

if [[ ! -f "${COBALT_STRIPPED_BIN}" ]]; then
  echo "Error: Stripped Cobalt binary not found at ${COBALT_STRIPPED_BIN}"
  exit 1
fi

if [[ "${DOWNLOAD_CHROMEDRIVER}" -eq 0 && ! -f "${CHROMEDRIVER_BIN}" ]]; then
  echo "Error: Chromedriver not found at ${CHROMEDRIVER_BIN}"
  exit 1
fi

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
REPO_ROOT="$(cd "${SKILL_DIR}/../../../.." &> /dev/null && pwd)"
TEST_SCRIPT="${SKILL_DIR}/test_main.py"

if [[ ! -f "${TEST_SCRIPT}" ]]; then
  echo "Error: test_main.py not found at ${TEST_SCRIPT}"
  exit 1
fi

VPYTHON_ARGS=()
if [[ -f "${COBALT_PATH}/.vpython3" ]]; then
  VPYTHON_ARGS+=(-vpython-spec "${COBALT_PATH}/.vpython3")
elif [[ -f "${REPO_ROOT}/.vpython3" ]]; then
  VPYTHON_ARGS+=(-vpython-spec "${REPO_ROOT}/.vpython3")
fi

XVFB_PID=""
DOWNLOAD_DIR="/tmp/chromedriver_download_$$"

cleanup() {
  echo "Cleaning up..."
  if [[ -n "${XVFB_PID}" ]] && kill -0 "${XVFB_PID}" 2>/dev/null; then
    kill "${XVFB_PID}" 2>/dev/null || true
  fi
  if [[ "${DOWNLOAD_CHROMEDRIVER}" -eq 1 ]]; then
    rm -rf "${DOWNLOAD_DIR}"
  fi
}
trap cleanup EXIT

if [[ "${DOWNLOAD_CHROMEDRIVER}" -eq 1 ]]; then
  echo "Downloading official Chromedriver..."
  if [[ -f "${COBALT_PATH}/chrome/VERSION" ]]; then
    MAJOR=$(grep 'MAJOR=' "${COBALT_PATH}/chrome/VERSION" | cut -d= -f2)
    MINOR=$(grep 'MINOR=' "${COBALT_PATH}/chrome/VERSION" | cut -d= -f2)
    BUILD=$(grep 'BUILD=' "${COBALT_PATH}/chrome/VERSION" | cut -d= -f2)
    PATCH=$(grep 'PATCH=' "${COBALT_PATH}/chrome/VERSION" | cut -d= -f2)
    VERSION="${MAJOR}.${MINOR}.${BUILD}.${PATCH}"
  elif [[ -f "${CHROMEDRIVER_BIN}" ]]; then
    VERSION=$("${CHROMEDRIVER_BIN}" --version | awk '{print $2}')
  else
    echo "Error: Cannot determine version for download."
    exit 1
  fi
  echo "Detected version: ${VERSION}"

  mkdir -p "${DOWNLOAD_DIR}"
  vpython3 "${VPYTHON_ARGS[@]}" "${SKILL_DIR}/download_chromedriver.py" "${VERSION}" --dest "${DOWNLOAD_DIR}"

  CHROMEDRIVER_BIN=$(find "${DOWNLOAD_DIR}" -name "chromedriver" -type f | head -n 1)
  if [[ -z "${CHROMEDRIVER_BIN}" ]]; then
    echo "Error: Failed to find downloaded chromedriver."
    exit 1
  fi
  chmod +x "${CHROMEDRIVER_BIN}"
  echo "Using downloaded driver: ${CHROMEDRIVER_BIN}"
fi

# Ensure Xvfb is running on XVFB_DISPLAY
if ! pgrep -f "Xvfb ${XVFB_DISPLAY}" > /dev/null; then
  echo "Starting Xvfb on display ${XVFB_DISPLAY}..."
  Xvfb "${XVFB_DISPLAY}" -noreset -nolisten tcp -ac -screen 0 "1920x1080x24" >/dev/null 2>&1 &
  XVFB_PID=$!
  sleep 2
else
  echo "Xvfb is already running on ${XVFB_DISPLAY}."
fi

# Ensure log and screenshot directories exist
mkdir -p "${OUTPUT_DIR}/logs" "${OUTPUT_DIR}/screenshots"

export LD_LIBRARY_PATH="${OUT_DIR}/starboard:${OUT_DIR}:${LD_LIBRARY_PATH:-}"

# Run the test
echo "Running smoke test..."
exit_code=0
DISPLAY="${XVFB_DISPLAY}" vpython3 "${VPYTHON_ARGS[@]}" "${TEST_SCRIPT}" \
  --binary "${COBALT_BIN}" \
  --driver "${CHROMEDRIVER_BIN}" \
  --output-dir "${OUTPUT_DIR}" \
  --verbose || exit_code=$?

if [[ "${exit_code}" -ne 0 ]]; then
  echo "Smoke test failed with exit code ${exit_code}. Dumping logs..."
  if [[ -f "${OUTPUT_DIR}/logs/chromedriver.log" ]]; then
    echo "=== chromedriver.log ==="
    tail -n 100 "${OUTPUT_DIR}/logs/chromedriver.log" || true
  fi
  if [[ -f "${OUTPUT_DIR}/logs/chrobalt.log" ]]; then
    echo "=== chrobalt.log ==="
    tail -n 100 "${OUTPUT_DIR}/logs/chrobalt.log" || true
  fi
  exit "${exit_code}"
fi

echo "Test completed successfully!"
