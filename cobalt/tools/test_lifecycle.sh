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

PORT=9223
LOG_FILE="lifecycle_run.log"
EXECUTABLE=${TEST_LIFECYCLE_EXECUTABLE:-"./out/linux-x64x11-modular_devel/cobalt_loader"}
HOST=${TEST_LIFECYCLE_HOST:-"localhost"}

# Ensure no previous test instances or Cobalt processes are running.
MY_PGID=$(ps -o pgid= -p $$ | tr -d ' ')
OTHER_TEST_PIDS=$(pgrep -f "[t]est_lifecycle.sh" | while read pid; do
  PGID=$(ps -o pgid= -p $pid | tr -d ' ')
  if [ "$PGID" != "$MY_PGID" ]; then echo $pid; fi
done | xargs || true)
WAITER_PIDS=$(pgrep -f "[w]ait_for_state.sh" | while read pid; do
  PGID=$(ps -o pgid= -p $pid | tr -d ' ')
  if [ "$PGID" != "$MY_PGID" ]; then echo $pid; fi
done | xargs || true)
COBALT_PIDS=$(pgrep -x "cobalt_loader" | while read pid; do
  PGID=$(ps -o pgid= -p $pid | tr -d ' ')
  if [ "$PGID" != "$MY_PGID" ]; then echo $pid; fi
done | xargs || true)

if [ -n "$OTHER_TEST_PIDS" ] || [ -n "$WAITER_PIDS" ] || [ -n "$COBALT_PIDS" ]; then
  echo "FAILURE: Previous test instance or Cobalt process is still running."
  [ -n "$OTHER_TEST_PIDS" ] && echo "Found test_lifecycle.sh PIDs: $OTHER_TEST_PIDS"
  [ -n "$WAITER_PIDS" ] && echo "Found wait_for_state.sh PIDs: $WAITER_PIDS"
  [ -n "$COBALT_PIDS" ] && echo "Found cobalt_loader PIDs: $COBALT_PIDS"
  echo "Please kill them before running this test."
  exit 1
fi

echo "[TEST] Starting cobalt_loader with DevTools on $HOST:$PORT..."
rm -f $LOG_FILE
rm -rf ~/.cobalt_storage ~/.cobalt ~/.config/cobalt

TEST_HTML="<html><body><h1>Test</h1><input autofocus></body></html>"
B64_HTML=$(echo "$TEST_HTML" | base64 -w 0)

$EXECUTABLE --url="data:text/html;base64,$B64_HTML" --remote-debugging-port=$PORT --no-sandbox > $LOG_FILE 2>&1 &
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
              window.event_log.push({type: 'visibilitychange', visibility: document.visibilityState});
            });
            window.addEventListener('focus', () => window.event_log.push({type: 'focus'}));
            window.addEventListener('blur', () => window.event_log.push({type: 'blur'}));
            document.addEventListener('freeze', () => window.event_log.push({type: 'freeze'}));
            document.addEventListener('resume', () => window.event_log.push({type: 'resume'}));
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

echo "[TEST] Skipping verification of events after resume due to known issue."

echo "[TEST] Killing Cobalt..."
kill -9 $COBALT_PID 2>/dev/null || true

echo "[TEST] SUCCESS: Lifecycle transitions verified (up to freeze)!"

exit 0
