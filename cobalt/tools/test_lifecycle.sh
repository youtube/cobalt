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

# Integration test for Cobalt lifecycle on Linux.
# Sends signals to a running Cobalt process and verifies JS state via DevTools.

if [[ -f cobalt/tools/test_common.sh ]]; then
  source cobalt/tools/test_common.sh
else
  MODE="modular"
  CONFIG="devel"
  parse_args() {
    shift
    while [[ $# -gt 0 ]]; do
      case $1 in
        --mode) MODE="$2"; shift 2 ;;
        --config) CONFIG="$2"; shift 2 ;;
        *) shift ;;
      esac
    done
    if [[ "$MODE" == "monolithic" ]]; then
      PLATFORM="linux-x64x11"
      EXECUTABLE_NAME="cobalt"
    else
      PLATFORM="linux-x64x11-modular"
      EXECUTABLE_NAME="cobalt_loader"
    fi
    EXECUTABLE=${TEST_LIFECYCLE_EXECUTABLE:-"./out/${PLATFORM}_${CONFIG}/${EXECUTABLE_NAME}"}
  }
  check_running_processes() { :; }
  run_build_if_needed() { :; }
fi

PORT=9223
LOG_FILE="lifecycle_run.log"
HOST=${TEST_LIFECYCLE_HOST:-"localhost"}

parse_args "test_lifecycle.sh" "$@"
check_running_processes "test_lifecycle.sh"
run_build_if_needed

echo "[TEST] Starting cobalt_loader with DevTools on $HOST:$PORT..."
rm -f $LOG_FILE
rm -rf ~/.cobalt_storage ~/.cobalt ~/.config/cobalt

TEST_HTML="<html><body><h1>Test</h1><input autofocus></body></html>"
B64_HTML=$(echo "$TEST_HTML" | base64 -w 0)

$EXECUTABLE --url="data:text/html;base64,$B64_HTML" --autoplay-policy=no-user-gesture-required --remote-debugging-port=$PORT --no-sandbox > $LOG_FILE 2>&1 &
COBALT_PID=$!

echo "[TEST] Launched PID: $COBALT_PID. Waiting for DevTools..."
if ! vpython3 cobalt/tools/cdp_js_helper.py --host $HOST --port $PORT --wait --wait-total 60 | grep -q "SUCCESS"; then
  echo "FAILURE: DevTools did not become ready in time."
  kill -9 $COBALT_PID
  exit 1
fi

echo "[TEST] Allowing time for app initialization to avoid V8 crashes..."
sleep 10

execute_js() {
  vpython3 cobalt/tools/cdp_js_helper.py --host $HOST --port $PORT "$1"
}

echo "[TEST] Verifying initial state (Visible & Focused)..."
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 120 $HOST || exit 1
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "True" $PORT 120 $HOST || exit 1

# Inject event logger now that we are in a stable initial state.
echo "[TEST] Injecting event logger..."
execute_js "window.event_log = [];
            document.addEventListener('visibilitychange', () => {
              console.log('JS_EVENT: visibilitychange ' + document.visibilityState);
              window.event_log.push({type: 'visibilitychange', visibility: document.visibilityState});
            });
            window.addEventListener('focus', () => {
              console.log('JS_EVENT: focus');
              window.event_log.push({type: 'focus'});
            });
            window.addEventListener('blur', () => {
              console.log('JS_EVENT: blur');
              window.event_log.push({type: 'blur'});
            });
            document.addEventListener('freeze', () => {
              console.log('JS_EVENT: freeze');
              window.event_log.push({type: 'freeze'});
            });
            document.addEventListener('resume', () => {
              console.log('JS_EVENT: resume');
              window.event_log.push({type: 'resume'});
            });
            window.focus();"

wait_and_pop_event() {
  local expected=$1
  echo "[WAIT] Waiting to pop event '$expected'..."
  bash cobalt/tools/wait_for_state.sh "window.event_log.length > 0" "True" $PORT 10 $HOST || exit 1
  local result=$(vpython3 cobalt/tools/cdp_js_helper.py --host $HOST --port $PORT "JSON.stringify(window.event_log.shift())" 2>/dev/null)
  if [[ "$result" == *"$expected"* ]]; then
    echo "[WAIT] SUCCESS: Popped expected event '$expected'"
  else
    echo "FAILURE: Expected '$expected', got '$result'"
    exit 1
  fi
}

echo "[TEST] Sending SIGWINCH (BLUR)..."
kill -SIGWINCH $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "False" $PORT 10 $HOST || exit 1
wait_and_pop_event '{"type":"blur"}'

echo "[TEST] Sending SIGCONT (FOCUS)..."
kill -SIGCONT $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "True" $PORT 10 $HOST || exit 1
wait_and_pop_event '{"type":"focus"}'

echo "[TEST] Sending SIGUSR1 (CONCEAL)..."
kill -SIGUSR1 $COBALT_PID
# Concealing from focused state will blur then change visibility to hidden
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "False" $PORT 10 $HOST || exit 1
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT 10 $HOST || exit 1
wait_and_pop_event '{"type":"blur"}'
wait_and_pop_event '{"type":"visibilitychange","visibility":"hidden"}'

echo "[TEST] Sending SIGTSTP (FREEZE)..."
kill -SIGTSTP $COBALT_PID

echo "[TEST] Sleeping to ensure app freezes..."
sleep 2

echo "[TEST] Sending SIGCONT (RESUME & REVEAL & FOCUS)..."
kill -SIGCONT $COBALT_PID
sleep 2

echo "[TEST] Verifying freeze and resume events in logs..."
grep -q "JS_EVENT: freeze" $LOG_FILE || { echo "FAILURE: freeze event not found in logs"; exit 1; }
grep -q "JS_EVENT: resume" $LOG_FILE || { echo "FAILURE: resume event not found in logs"; exit 1; }
grep -q "JS_EVENT: visibilitychange visible" $LOG_FILE || { echo "FAILURE: visible event not found in logs"; exit 1; }
[ $(grep -c "JS_EVENT: focus" $LOG_FILE) -ge 2 ] || { echo "FAILURE: expected at least 2 focus events in logs"; exit 1; }

# ---------------------------------------------------------------------------
# Media Playback Conceal/Reveal Counter-Test
# Verifies:
#   1. Active playback on conceal -> auto-pauses, destroys SbPlayer BEFORE
#      "Transition to kConcealed complete", and auto-resumes on reveal.
#   2. Web-app-paused playback on conceal -> destroys SbPlayer BEFORE
#      "Transition to kConcealed complete", and stays paused on reveal.
#   3. Web-app-removed <video> on conceal -> destroys SbPlayer BEFORE
#      "Transition to kConcealed complete".
# ---------------------------------------------------------------------------
echo "[TEST] Starting Media Playback Conceal/Reveal Counter-Test..."
WEBM_B64=$(base64 -w 0 media/test/data/four-colors-vp9.webm)

# Helper to verify that the Nth occurrence of "Destroying SbPlayerPrivateImpl"
# appears strictly before the Mth occurrence of "Transition to kConcealed complete"
# in $LOG_FILE.
verify_sbplayer_destroyed_before_conceal_complete() {
  local destroy_nth=$1
  local conceal_nth=$2
  local label=$3
  local destroy_line=""
  local conceal_line=""

  # Wait up to 10s for "Transition to kConcealed complete" #conceal_nth to be flushed.
  # Anchor with '$' to exclude intermediate "Transition to kConcealed complete (target: kStarted)" logs on resume.
  for _ in $(seq 1 20); do
    conceal_line=$(grep -n "Transition to kConcealed complete$" "$LOG_FILE" | sed -n "${conceal_nth}p" | cut -d: -f1)
    if [[ -n "$conceal_line" ]]; then
      break
    fi
    sleep 0.5
  done

  destroy_line=$(grep -n "Destroying SbPlayerPrivateImpl. There are 0 players." "$LOG_FILE" | sed -n "${destroy_nth}p" | cut -d: -f1)

  if [[ -z "$conceal_line" ]]; then
    echo "FAILURE [$label]: 'Transition to kConcealed complete' (#$conceal_nth) not found in $LOG_FILE"
    exit 1
  fi
  if [[ -z "$destroy_line" ]]; then
    echo "FAILURE [$label]: Counter-test caught bug! 'Destroying SbPlayerPrivateImpl' (#$destroy_nth) was NEVER logged before conceal completed (line $conceal_line)"
    exit 1
  fi
  if [[ "$destroy_line" -ge "$conceal_line" ]]; then
    echo "FAILURE [$label]: Counter-test caught race! 'Destroying SbPlayerPrivateImpl' (line $destroy_line) occurred AFTER 'Transition to kConcealed complete' (line $conceal_line)"
    exit 1
  fi
  echo "[TEST] SUCCESS [$label]: SbPlayerDestroy (line $destroy_line) < Transition to kConcealed complete (line $conceal_line)"
}

# --- Scenario 1: Active playback (app does NOT pause or destroy <video>) ---
echo "[TEST] Scenario 1: Launching MSE VP9 playback..."
execute_js "
  window.startTestVideo = async () => {
    const existing = document.getElementById('test_video');
    if (existing) existing.remove();
    const v = document.createElement('video');
    v.id = 'test_video';
    v.loop = true;
    v.muted = true;
    document.body.appendChild(v);
    const ms = new MediaSource();
    v.src = URL.createObjectURL(ms);
    await new Promise(r => ms.addEventListener('sourceopen', r, {once: true}));
    const sb = ms.addSourceBuffer('video/webm; codecs=\"vp9\"');
    const raw = atob('$WEBM_B64');
    const bytes = new Uint8Array(raw.length);
    for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
    sb.appendBuffer(bytes);
    await new Promise(r => sb.addEventListener('updateend', r, {once: true}));
    ms.endOfStream();
    await v.play();
    return true;
  };
  window.startTestVideo();
"

bash cobalt/tools/wait_for_state.sh \
  "(() => { const v = document.getElementById('test_video'); return Boolean(v && !v.paused && v.readyState >= 2); })()" \
  "True" $PORT 20 $HOST || exit 1

grep -q "Creating SbPlayerPrivateImpl. There are 1 players." "$LOG_FILE" || {
  echo "FAILURE [Scenario 1]: SbPlayerPrivateImpl was not created"
  exit 1
}

# Note: 1st conceal in test_lifecycle.sh was at line 106 (without media).
# Therefore this media conceal is conceal #2, and SbPlayerDestroy #1.
echo "[TEST] Scenario 1: Sending SIGUSR1 (CONCEAL) during active playback..."
kill -SIGUSR1 $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT 10 $HOST || exit 1

verify_sbplayer_destroyed_before_conceal_complete 1 2 "Scenario 1"

echo "[TEST] Scenario 1: Sending SIGCONT (REVEAL) and verifying auto-resume..."
kill -SIGCONT $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 10 $HOST || exit 1
bash cobalt/tools/wait_for_state.sh "document.getElementById('test_video').paused" "False" $PORT 10 $HOST || exit 1

# --- Scenario 2: Web app explicitly calls video.pause() on conceal ---
echo "[TEST] Scenario 2: Registering visibilitychange listener that pauses video..."
execute_js "
  document.addEventListener('visibilitychange', function onHidePause() {
    if (document.visibilityState === 'hidden') {
      document.removeEventListener('visibilitychange', onHidePause);
      document.getElementById('test_video').pause();
    }
  });
"

echo "[TEST] Scenario 2: Sending SIGUSR1 (CONCEAL)..."
kill -SIGUSR1 $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT 10 $HOST || exit 1

verify_sbplayer_destroyed_before_conceal_complete 2 3 "Scenario 2"

echo "[TEST] Scenario 2: Sending SIGCONT (REVEAL) and verifying video stays PAUSED..."
kill -SIGCONT $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 10 $HOST || exit 1
sleep 1
bash cobalt/tools/wait_for_state.sh "document.getElementById('test_video').paused" "True" $PORT 5 $HOST || exit 1

# --- Scenario 3: Web app removes <video> on conceal ---
echo "[TEST] Scenario 3: Resuming playback and registering visibilitychange listener that removes <video>..."
execute_js "
  document.getElementById('test_video').play();
  document.addEventListener('visibilitychange', function onHideRemove() {
    if (document.visibilityState === 'hidden') {
      document.removeEventListener('visibilitychange', onHideRemove);
      const v = document.getElementById('test_video');
      if (v) {
        v.removeAttribute('src');
        v.load();
        v.remove();
      }
    }
  });
"
bash cobalt/tools/wait_for_state.sh "document.getElementById('test_video').paused" "False" $PORT 10 $HOST || exit 1

echo "[TEST] Scenario 3: Sending SIGUSR1 (CONCEAL)..."
kill -SIGUSR1 $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT 10 $HOST || exit 1

verify_sbplayer_destroyed_before_conceal_complete 3 4 "Scenario 3"

echo "[TEST] Killing Cobalt..."
kill -9 $COBALT_PID 2>/dev/null || true

echo "[TEST] SUCCESS: Lifecycle transitions verified!"

exit 0
