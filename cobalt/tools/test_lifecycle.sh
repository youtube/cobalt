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

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

function log {
  echo -e "${GREEN}[TEST] $(date +'%H:%M:%S') - $1${NC}"
}

# Ensure no previous test instances or Cobalt processes are running.
pgrep -f "[t]est_lifecycle.sh" > /tmp/test_lifecycle_pids.tmp || true
OTHER_TEST_PIDS=""
while read pid; do
  if [ -n "$pid" ] && [ "$pid" != "$$" ] && [ "$pid" != "$PPID" ]; then
    OTHER_TEST_PIDS="$OTHER_TEST_PIDS $pid"
  fi
done < /tmp/test_lifecycle_pids.tmp
OTHER_TEST_PIDS=${OTHER_TEST_PIDS# }

WAITER_PIDS=$(pgrep -f "[w]ait_for_state.sh" | grep -vw $$ | grep -vw $PPID || true)
COBALT_PIDS=$(pgrep -f "[c]obalt_loader" | grep -vw $$ | grep -vw $PPID || true)

if [ -n "$OTHER_TEST_PIDS" ] || [ -n "$WAITER_PIDS" ] || [ -n "$COBALT_PIDS" ]; then
  echo "FAILURE: Previous test instance or Cobalt process is still running."
  [ -n "$OTHER_TEST_PIDS" ] && echo "Found test_lifecycle.sh PIDs: $OTHER_TEST_PIDS"
  [ -n "$WAITER_PIDS" ] && echo "Found wait_for_state.sh PIDs: $WAITER_PIDS"
  [ -n "$COBALT_PIDS" ] && echo "Found cobalt_loader PIDs: $COBALT_PIDS"
  echo "--- ps faux dump ---"
  ps faux | grep -C 5 "test_lifecycle"
  echo "--------------------"
  echo "Current PID: $$"
  echo "Parent PID: $PPID"
  echo "BASHPID: $BASHPID"
  echo "Please kill them before running this test."
  exit 1
fi

log "Starting cobalt_loader with DevTools on port $PORT..."
rm -f $LOG_FILE

$EXECUTABLE --remote-debugging-port=$PORT --no-sandbox --v=1 > $LOG_FILE 2>&1 &
COBALT_PID=$!

log "Launched PID: $COBALT_PID. Waiting for DevTools..."
sleep 5

log "Verifying initial state (Visible & Focused)..."
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 120 || exit 1
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "True" $PORT 120 || exit 1

log "Sending SIGWINCH (BLUR)..."
kill -SIGWINCH $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "False" $PORT || exit 1

log "Sending SIGCONT (FOCUS)..."
kill -SIGCONT $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "True" $PORT || exit 1

log "Sending SIGUSR1 (CONCEAL)..."
kill -SIGUSR1 $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "hidden" $PORT || exit 1

log 'Wait 3 seconds, manually confirm that the window has disappeared.'
sleep 3

log "Sending SIGCONT (REVEAL & FOCUS)..."
kill -SIGCONT $COBALT_PID
bash cobalt/tools/wait_for_state.sh "document.visibilityState" "visible" $PORT 120 || exit 1
bash cobalt/tools/wait_for_state.sh "document.hasFocus()" "True" $PORT 120 || exit 1

log 'Wait 3 seconds, manually confirm that the window has returned.'
sleep 3

log "Sending SIGPWR (STOP)..."
kill -SIGPWR $COBALT_PID
sleep 5

if kill -0 $COBALT_PID 2>/dev/null; then
  STAT=$(ps -p $COBALT_PID -o stat= | tr -d ' ')
  if [[ "$STAT" != "Z"* ]]; then
    echo "FAILURE: Cobalt (PID: $COBALT_PID) did not stop after SIGPWR. State: $STAT"
    echo "[TEST] Cleaning up Cobalt (PID: $COBALT_PID)..."
    kill -9 $COBALT_PID
    exit 1
  fi
fi

log "SUCCESS: Lifecycle transitions verified!"

exit 0
