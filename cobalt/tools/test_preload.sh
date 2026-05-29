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

# Integration test for Cobalt Preload on Linux.
# Verifies that Cobalt starts in a hidden state and reveals on SIGCONT.

PORT=9223
HOST=${TEST_PRELOAD_HOST:-"localhost"}
LOG_FILE="preload_run.log"
EXECUTABLE=${TEST_PRELOAD_EXECUTABLE:-"./out/linux-x64x11-modular_devel/cobalt_loader"}

# Ensure no previous test instances or Cobalt processes are running.
pgrep -f "[t]est_preload.sh" > /tmp/test_preload_pids.tmp || true
OTHER_TEST_PIDS=""
while read pid; do
  if [ -n "$pid" ] && [ "$pid" != "$$" ] && [ "$pid" != "$PPID" ]; then
    OTHER_TEST_PIDS="$OTHER_TEST_PIDS $pid"
  fi
done < /tmp/test_preload_pids.tmp
OTHER_TEST_PIDS=${OTHER_TEST_PIDS# }

WAITER_PIDS=$(pgrep -f "[w]ait_for_state.sh" | grep -vw $$ | grep -vw $PPID || true)
COBALT_PIDS=$(pgrep -f "[c]obalt_loader" | grep -vw $$ | grep -vw $PPID || true)

if [ -n "$OTHER_TEST_PIDS" ] || [ -n "$WAITER_PIDS" ] || [ -n "$COBALT_PIDS" ]; then
  echo "FAILURE: Previous test instance or Cobalt process is still running."
  [ -n "$OTHER_TEST_PIDS" ] && echo "Found test_preload.sh PIDs: $OTHER_TEST_PIDS"
  [ -n "$WAITER_PIDS" ] && echo "Found wait_for_state.sh PIDs: $WAITER_PIDS"
  [ -n "$COBALT_PIDS" ] && echo "Found cobalt_loader PIDs: $COBALT_PIDS"
  echo "Please kill them before running this test."
  exit 1
fi

echo "[TEST] Starting cobalt_loader in preload mode with DevTools on $HOST:$PORT..."
rm -f $LOG_FILE

$EXECUTABLE --preload --remote-debugging-port=$PORT --no-sandbox > $LOG_FILE 2>&1 &
COBALT_PID=$!

echo "[TEST] Launched PID: $COBALT_PID. Waiting for DevTools..."
if ! vpython3 cobalt/tools/cdp_js_helper.py --host $HOST --port $PORT --wait --wait-total 60 | grep -q "SUCCESS"; then
  echo "FAILURE: DevTools did not become ready in time."
  kill -9 $COBALT_PID
  exit 1
fi

execute_js() {
  vpython3 cobalt/tools/cdp_js_helper.py --host $HOST --port $PORT "$1"
}

# Inject event logger.
echo "[TEST] Injecting event logger..."
execute_js "window.event_log = [];
            document.addEventListener('visibilitychange', () => {
              window.event_log.push({type: 'visibilitychange', visibility: document.visibilityState, prerendering: document.prerendering});
            });
            document.addEventListener('prerenderingchange', () => {
              window.event_log.push({type: 'prerenderingchange', visibility: document.visibilityState, prerendering: document.prerendering});
            });"

echo "[TEST] Verifying initial preloaded state (Hidden)..."
# In preload mode, the app should be hidden.
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT 120 $HOST || exit 1
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "False" $PORT 120 $HOST || exit 1

echo "[TEST] Sending SIGCONT to reveal application..."
kill -SIGCONT $COBALT_PID

echo "[TEST] Verifying revealed state (Visible)..."
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 120 $HOST || exit 1
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "True" $PORT 120 $HOST || exit 1

echo "[TEST] Verifying event sequence..."
# Expected: visibilitychange (visible)
EXPECTED_VISIBILITY_EVENT="window.event_log.some(e => e.type === 'visibilitychange' && e.visibility === 'visible')"
bash cobalt/tools/wait_for_state.sh "$EXPECTED_VISIBILITY_EVENT" "True" $PORT 60 $HOST || exit 1

echo "[TEST] Testing conceal/reveal cycle after initial reveal..."
execute_js "window.event_log = [];"

echo "[TEST] Sending SIGUSR1 to conceal..."
kill -SIGUSR1 $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT 120 $HOST || exit 1

echo "[TEST] Sending SIGCONT to reveal..."
kill -SIGCONT $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 120 $HOST || exit 1

echo "[TEST] Verifying cycle events..."
EXPECTED_CONCEAL_EVENT="window.event_log.some(e => e.type === 'visibilitychange' && e.visibility === 'hidden')"
bash cobalt/tools/wait_for_state.sh "$EXPECTED_CONCEAL_EVENT" "True" $PORT 120 $HOST || exit 1

EXPECTED_REVEAL_EVENT="window.event_log.some(e => e.type === 'visibilitychange' && e.visibility === 'visible')"
bash cobalt/tools/wait_for_state.sh "$EXPECTED_REVEAL_EVENT" "True" $PORT 120 $HOST || exit 1

echo "[TEST] Sending SIGPWR (STOP)..."
kill -SIGPWR $COBALT_PID
sleep 10

if kill -0 $COBALT_PID 2>/dev/null; then
  STAT=$(ps -p $COBALT_PID -o stat= | tr -d ' ')
  if [[ "$STAT" != "Z"* ]]; then
    echo "FAILURE: Cobalt (PID: $COBALT_PID) did not stop after SIGPWR. State: $STAT"
    echo "[TEST] Cleaning up Cobalt (PID: $COBALT_PID)..."
    kill -9 $COBALT_PID
    exit 1
  fi
fi

echo "[TEST] SUCCESS: Preload and reveal verified!"

exit 0
