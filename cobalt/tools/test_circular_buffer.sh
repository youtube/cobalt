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

# E2E test for Cobalt H5vccCircularBuffer crash resilience.
# Runs Cobalt, writes data, kills it, restarts, and verifies recovery.

PORT=9224
LOG_FILE="circular_buffer_test.log"
EXECUTABLE=${TEST_CIRCULAR_BUFFER_EXECUTABLE:-"./out/linux-x64x11-modular_devel/cobalt_loader"}

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
DEMO_URL="file://${SCRIPT_DIR}/../browser/h5vcc_circular_buffer/demo_e2e.html"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

function log {
  echo -e "${GREEN}[TEST] $(date +'%H:%M:%S') - $1${NC}"
}

# Ensure no previous instances are running
COBALT_PIDS=$(pgrep -f "[c]obalt_loader" || true)
if [ -n "$COBALT_PIDS" ]; then
  log "Cleaning up old Cobalt processes: $COBALT_PIDS"
  kill -9 $COBALT_PIDS
fi

log "Phase 1: Starting Cobalt to write data..."
$EXECUTABLE --remote-debugging-port=$PORT --no-sandbox --v=1 "$DEMO_URL" > $LOG_FILE 2>&1 &
COBALT_PID=$!

log "Launched PID: $COBALT_PID. Waiting for data to be written..."
bash cobalt/tools/wait_for_state.sh "window.test_status" "WRITTEN" $PORT 30 || {
  log "${RED}Failed to write data or timeout.${NC}"
  kill -9 $COBALT_PID
  exit 1
}

log "Data written. Hard killing Cobalt with kill -9..."
kill -9 $COBALT_PID
sleep 2

log "Phase 2: Restarting Cobalt to verify recovery..."
$EXECUTABLE --remote-debugging-port=$PORT --no-sandbox --v=1 "$DEMO_URL" >> $LOG_FILE 2>&1 &
COBALT_PID=$!

log "Launched PID: $COBALT_PID. Waiting for recovery verification..."
bash cobalt/tools/wait_for_state.sh "window.test_status" "SUCCESS" $PORT 30 || {
  log "${RED}Failed to recover data or timeout.${NC}"
  kill -9 $COBALT_PID
  exit 1
}

log "SUCCESS: Data recovered successfully after crash!"
kill -9 $COBALT_PID

exit 0
