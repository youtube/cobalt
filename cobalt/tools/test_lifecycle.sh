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

# Integration test for Cobalt lifecycle signals and Visibility/Focus APIs.

set -e

# Configuration
PLATFORM="linux-x64x11-modular"
CONFIG="devel"
TARGET="cobalt_loader"
OUT_DIR="out/${PLATFORM}_${CONFIG}"
EXECUTABLE="${TEST_LIFECYCLE_EXECUTABLE:-./${OUT_DIR}/${TARGET}}"
LOG_FILE="lifecycle_run.log"
PORT=9222

# Colors for logging
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

# Ensure cobalt is killed on exit if test fails.
function cleanup {
  if [ -n "$COBALT_PID" ] && kill -0 $COBALT_PID 2>/dev/null; then
    log "Cleaning up Cobalt (PID: $COBALT_PID)..."
    kill -9 $COBALT_PID 2>/dev/null || true
  fi
}
trap cleanup EXIT

if [[ ! -f "$EXECUTABLE" ]]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE${NC}"
    exit 1
fi

log "Starting $TARGET with DevTools on port $PORT..."
$EXECUTABLE --remote-debugging-port=$PORT --no-sandbox --v=1 > $LOG_FILE 2>&1 &
COBALT_PID=$!

log "Launched PID: $COBALT_PID. Waiting for DevTools..."
MAX_TRIES=30
TRIES=0
until curl -s http://localhost:$PORT/json > /dev/null; do
  sleep 1
  TRIES=$((TRIES + 1))
  if [ $TRIES -eq $MAX_TRIES ]; then
    echo -e "${RED}FAILURE: Cobalt DevTools did not become ready in $MAX_TRIES seconds.${NC}"
    tail -n 20 "$LOG_FILE"
    exit 1
  fi
done

# Helper to check JS state
function check_js {
  vpython3 cobalt/tools/cdp_js_helper.py --port $PORT "$1"
}

log "Verifying initial state (Visible & Focused)..."
sleep 2
# Force focus via JS just in case.
check_js "window.focus()" > /dev/null
VISIBILITY=$(check_js "document.visibilityState")
HAS_FOCUS=$(check_js "document.hasFocus()" | tr '[:upper:]' '[:lower:]')
log "Visibility: $VISIBILITY, Focused: $HAS_FOCUS"

if [ "$VISIBILITY" != "visible" ] || [ "$HAS_FOCUS" != "true" ]; then
  echo -e "${RED}FAILURE: Initial state should be visible and focused.${NC}"
  exit 1
fi

log "Sending SIGWINCH (BLUR)..."
kill -SIGWINCH $COBALT_PID
sleep 2
VISIBILITY=$(check_js "document.visibilityState")
HAS_FOCUS=$(check_js "document.hasFocus()" | tr '[:upper:]' '[:lower:]')
log "Visibility: $VISIBILITY, Focused: $HAS_FOCUS"
if [ "$HAS_FOCUS" != "false" ]; then
  echo -e "${RED}FAILURE: Should have lost focus after SIGWINCH.${NC}"
  exit 1
fi

log "Sending SIGCONT (FOCUS)..."
kill -SIGCONT $COBALT_PID
sleep 2
HAS_FOCUS=$(check_js "document.hasFocus()" | tr '[:upper:]' '[:lower:]')
log "Focused: $HAS_FOCUS"
if [ "$HAS_FOCUS" != "true" ]; then
  echo -e "${RED}FAILURE: Should have regained focus after SIGCONT.${NC}"
  exit 1
fi

log "Sending SIGUSR1 (CONCEAL)..."
kill -SIGUSR1 $COBALT_PID
sleep 5
VISIBILITY=$(check_js "document.visibilityState")
log "Visibility: $VISIBILITY"
if [ "$VISIBILITY" != "hidden" ]; then
  echo -e "${RED}FAILURE: Should be hidden after SIGUSR1.${NC}"
  exit 1
fi

log "Sending SIGCONT (REVEAL & FOCUS)..."
kill -SIGCONT $COBALT_PID
sleep 2
VISIBILITY=$(check_js "document.visibilityState")
HAS_FOCUS=$(check_js "document.hasFocus()" | tr '[:upper:]' '[:lower:]')
log "Visibility: $VISIBILITY, Focused: $HAS_FOCUS"
if [ "$VISIBILITY" != "visible" ] || [ "$HAS_FOCUS" != "true" ]; then
  echo -e "${RED}FAILURE: Should be visible and focused after SIGCONT.${NC}"
  exit 1
fi

log "Sending SIGTSTP (FREEZE)..."
kill -SIGTSTP $COBALT_PID
sleep 2
# Verifying behavior (visibility API) after SIGTSTP
VISIBILITY=$(check_js "document.visibilityState")
log "Visibility: $VISIBILITY"
if [ "$VISIBILITY" != "hidden" ]; then
  echo -e "${RED}FAILURE: Should be hidden after SIGTSTP (which implies Conceal).${NC}"
  exit 1
fi

log "Sending SIGCONT (UNFREEZE & REVEAL & FOCUS)..."
kill -SIGCONT $COBALT_PID
sleep 2
VISIBILITY=$(check_js "document.visibilityState")
HAS_FOCUS=$(check_js "document.hasFocus()" | tr '[:upper:]' '[:lower:]')
log "Visibility: $VISIBILITY, Focused: $HAS_FOCUS"

if [ "$VISIBILITY" != "visible" ] || [ "$HAS_FOCUS" != "true" ]; then
  echo -e "${RED}FAILURE: Should be visible and focused after SIGCONT from Frozen.${NC}"
  exit 1
fi

log "Sending SIGPWR (STOP/QUIT)..."
kill -SIGPWR $COBALT_PID

# Wait for process to exit
set +e
wait $COBALT_PID
EXIT_CODE=$?
set -e

log "Cobalt exited with code: $EXIT_CODE"
log "Reviewing logs for fatal errors..."

FILTERED_LOG=$(grep -aE "FATAL|AddressSanitizer|leak|Check failed" "$LOG_FILE" | grep -av "MojoDiscardableSharedMemoryManagerImpls are still alive" | grep -av "lock_impl_posix.cc(46)" || true)
if [[ -n "$FILTERED_LOG" ]]; then
    echo -e "${RED}FAILURE: Found errors in log.${NC}"
    echo "$FILTERED_LOG" | head -n 20
    exit 1
fi

echo -e "${GREEN}SUCCESS: Lifecycle integration test passed.${NC}"
