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
#   empty: delete the HTTP cache before launch. Cookies, login, and the V8
#          code cache are kept. Note: Cobalt's HTTP cache only stores
#          html/js/css/wasm; images and fonts are always downloaded.
#
# Pass: cold launch -> normalize to HOME (handles the account picker) ->
#       5 DOWN + 5 UP (R2R) -> 5 RIGHT + 4 LEFT (T2T) -> OK (play) -> BACK ->
#       HOME -> warm launch.
#
# With multiple adb devices, set ANDROID_SERIAL to pick one.
set -euo pipefail

OUT_DIR="${1:?Usage: run_once.sh <out_dir> [warm|empty]}"
CACHE_MODE="${2:-warm}"
PKG=dev.cobalt.coat
ACT=dev.cobalt.coat/dev.cobalt.app.MainActivity
mkdir -p "${OUT_DIR}"

key() { adb shell input keyevent "$1"; sleep "${2:-1.5}"; }
mark() {
  echo "HARNESS $1 host_time=$(date +%s.%N)" >> "${OUT_DIR}/harness.log"
  # Phase marker in logcat; parse_atom_perf.py groups events by these markers.
  adb shell log -t ATOM_PERF "ATOM_PERF phase name=$1"
}

echo "cache_mode=${CACHE_MODE}" > "${OUT_DIR}/config.txt"
adb logcat -G 16M >/dev/null 2>&1 || true
adb shell am force-stop "${PKG}"
sleep 2
# The app deletes its HTTP cache before native init when launched with
# atomClearHttpCache (works on non-debuggable builds, unlike run-as).
COLD_EXTRAS=()
if [[ "${CACHE_MODE}" == "empty" ]]; then
  COLD_EXTRAS=(--ez atomClearHttpCache true)
fi
adb logcat -c

# 1. Cold launch.
mark cold_launch
adb shell am start -W -n "${ACT}" ${COLD_EXTRAS[@]+"${COLD_EXTRAS[@]}"} > "${OUT_DIR}/am_start_cold.txt"
sleep 25  # Let the home page settle (thumbnails load).
adb shell dumpsys meminfo "${PKG}" > "${OUT_DIR}/meminfo_home.txt"

# 2. Normalize to HOME. The app occasionally starts on the account picker.
#   SELECT: picker -> choose 1st account, go HOME | home -> start 1st video
#   SELECT: picker path -> pause video            | home path -> start video
#   PLAY:   picker path -> resume video           | home path -> no-op
#   BACK:   both paths -> HOME
mark normalize
key KEYCODE_DPAD_CENTER 8
key KEYCODE_DPAD_CENTER 5
key KEYCODE_MEDIA_PLAY 5
key KEYCODE_BACK 5

# 3. Row-to-row (R2R): move down and back up through the home shelves.
mark r2r
for _ in 1 2 3 4 5; do key KEYCODE_DPAD_DOWN 2; done
for _ in 1 2 3 4 5; do key KEYCODE_DPAD_UP 2; done

# 4. Tile-to-tile (T2T) / input latency: D-pad moves on home. Ends one tile right of the start, so
#    browse-to-watch opens a video that the normalize step did not play
#    (replaying it resumes at the last position, which adds a seek).
mark dpad
for _ in 1 2 3 4 5; do key KEYCODE_DPAD_RIGHT; done
for _ in 1 2 3 4; do key KEYCODE_DPAD_LEFT; done

# 5. Browse-to-watch: select the focused thumbnail.
mark browse_to_watch
key KEYCODE_DPAD_CENTER 15
adb shell dumpsys meminfo "${PKG}" > "${OUT_DIR}/meminfo_playback.txt"

# 6. Warm launch: back to home, go to the launcher, then relaunch.
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
