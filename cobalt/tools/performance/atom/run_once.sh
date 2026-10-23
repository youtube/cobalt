#!/usr/bin/env bash
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
#
# ATOM_PERF (PoC): Runs one measurement pass of Cobalt on the connected adb
# device and saves logcat for parse_atom_perf.py.
#
# Usage: run_once.sh <out_dir> [warm|empty]
#   warm  (default): keep the HTTP cache (relaunch shortly after last use).
#   empty: delete the HTTP cache before launch (worst case: all resources are
#          downloaded again). Cookies, login, and the V8 code cache are kept.
#
# Pass: cold launch -> 10 D-pad presses -> OK (play) -> BACK -> HOME ->
#       warm launch.
set -euo pipefail

OUT_DIR="${1:?Usage: run_once.sh <out_dir> [warm|empty]}"
CACHE_MODE="${2:-warm}"
PKG=dev.cobalt.coat
ACT=dev.cobalt.coat/dev.cobalt.app.MainActivity
HTTP_CACHE_DIR="/data/data/${PKG}/cache/Cache"
mkdir -p "${OUT_DIR}"

key() { adb shell input keyevent "$1"; sleep "${2:-1.5}"; }
mark() { echo "HARNESS $1 host_time=$(date +%s.%N)" >> "${OUT_DIR}/harness.log"; }

echo "cache_mode=${CACHE_MODE}" > "${OUT_DIR}/config.txt"
adb logcat -G 16M >/dev/null 2>&1 || true
adb shell am force-stop "${PKG}"
sleep 2
if [[ "${CACHE_MODE}" == "empty" ]]; then
  adb shell "run-as ${PKG} rm -rf ${HTTP_CACHE_DIR}"
fi
adb logcat -c

# 1. Cold launch.
mark cold_launch
adb shell am start -W -n "${ACT}" > "${OUT_DIR}/am_start_cold.txt"
sleep 25  # Let the home page settle (thumbnails load).
adb shell dumpsys meminfo "${PKG}" > "${OUT_DIR}/meminfo_home.txt"

# 2. Input latency: D-pad moves on home.
mark dpad
for _ in 1 2 3 4 5; do key KEYCODE_DPAD_RIGHT; done
for _ in 1 2 3 4 5; do key KEYCODE_DPAD_LEFT; done

# 3. Browse-to-watch: select the focused thumbnail.
mark browse_to_watch
key KEYCODE_DPAD_CENTER 15
adb shell dumpsys meminfo "${PKG}" > "${OUT_DIR}/meminfo_playback.txt"

# 4. Warm launch: back to home, go to the launcher, then relaunch.
mark back_and_home
key KEYCODE_BACK 5
key KEYCODE_HOME 8
mark warm_launch
adb shell am start -W -n "${ACT}" > "${OUT_DIR}/am_start_warm.txt"
sleep 10

mark done
adb logcat -d -v threadtime > "${OUT_DIR}/logcat_full.txt"
grep "ATOM_PERF" "${OUT_DIR}/logcat_full.txt" > "${OUT_DIR}/atom_perf.log" || true
echo "Saved ${OUT_DIR} ($(wc -l < "${OUT_DIR}/atom_perf.log") ATOM_PERF lines)"
