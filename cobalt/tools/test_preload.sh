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

# Integration test for Cobalt --preload flag and Visibility API.

set -e

# Path to cobalt binary.
PLATFORM="linux-x64x11-modular"
CONFIG="devel"
TARGET="cobalt_loader"
DEFAULT_COBALT_BIN="out/${PLATFORM}_${CONFIG}/${TARGET}"

COBALT_BIN=${1:-$DEFAULT_COBALT_BIN}
if [ ! -f "$COBALT_BIN" ]; then
  echo -e "${RED}Error: Cobalt binary not found at $COBALT_BIN${NC}"
  echo "Usage: $0 <path_to_cobalt_bin>"
  exit 1
fi

PORT=9222
LOG_FILE="preload_test.log"

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

log "Starting Cobalt in preload mode..."
$COBALT_BIN \
  --preload \
  --remote-debugging-port=$PORT \
  --no-sandbox > "$LOG_FILE" 2>&1 &
COBALT_PID=$!

log "Launched PID: $COBALT_PID. Waiting for DevTools on port $PORT..."
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
  CDP_PORT=$PORT vpython3 cobalt/tools/cdp_js_helper.py "$1"
}

log "Verifying initial preloaded state..."
VISIBILITY=$(check_js "document.visibilityState")
log "Visibility state: $VISIBILITY"

if [ "$VISIBILITY" != "hidden" ]; then
  echo -e "${RED}FAILURE: Initial visibilityState should be 'hidden', got '$VISIBILITY'${NC}"
  exit 1
fi

# Send reveal event.
log "Sending SIGCONT to reveal application..."
kill -SIGCONT $COBALT_PID

# Wait for state change.
sleep 2

log "Verifying revealed state..."
VISIBILITY=$(check_js "document.visibilityState")
log "Visibility state: $VISIBILITY"

if [ "$VISIBILITY" != "visible" ]; then
  echo -e "${RED}FAILURE: VisibilityState should be 'visible' after reveal, got '$VISIBILITY'${NC}"
  exit 1
fi

log "Sending SIGPWR to shutdown application..."
kill -SIGPWR $COBALT_PID

# Wait for process to exit
log "Waiting for Cobalt to exit..."
wait $COBALT_PID
EXIT_CODE=$?

log "Cobalt exited with code: $EXIT_CODE"

if [ $EXIT_CODE -ne 0 ]; then
  echo -e "${RED}FAILURE: Cobalt exited with non-zero code $EXIT_CODE${NC}"
  exit 1
fi

# Check for fatal errors in log.
FILTERED_LOG=$(grep -E "FATAL|AddressSanitizer|leak|Check failed" "$LOG_FILE" | grep -v "MojoDiscardableSharedMemoryManagerImpls are still alive" || true)
if [[ -n "$FILTERED_LOG" ]]; then
    echo -e "${RED}FAILURE: Found errors in log.${NC}"
    echo "$FILTERED_LOG" | head -n 20
    exit 1
fi

echo -e "${GREEN}SUCCESS${NC}"
exit 0

# Example output of a passing test:
# [TEST] Starting Cobalt in preload mode...
# [TEST] Launched PID: 1711948. Waiting for DevTools on port 9222...
# [TEST] Verifying initial preloaded state...
# [TEST] Visibility state: hidden
# [TEST] Sending SIGCONT to reveal application...
# [TEST] Verifying revealed state...
# [TEST] Visibility state: visible
# [TEST] Sending SIGPWR to shutdown application...
# [TEST] Waiting for Cobalt to exit...
# [TEST] Cobalt exited with code: 0
# SUCCESS
