#!/bin/bash

# Configuration
PLATFORM="linux-x64x11-modular"
CONFIG="devel"
TARGET="cobalt_loader"
OUT_DIR="out/${PLATFORM}_${CONFIG}"
EXECUTABLE="./${OUT_DIR}/${TARGET}"

# Colors for logging
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

if [[ ! -f "$EXECUTABLE" ]]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE${NC}"
    exit 1
fi

log "Starting $TARGET in background..."
# Run in background, redirect output to a file
$EXECUTABLE > tasks/lifecycle_run.log 2>&1 &
COBALT_PID=$!

log "Launched PID: $COBALT_PID. Waiting for initialization (10s)..."
sleep 10

if ! kill -0 $COBALT_PID 2>/dev/null; then
    echo -e "${RED}Error: Cobalt process died during startup.${NC}"
    tail -n 20 tasks/lifecycle_run.log
    exit 1
fi

log "Sending SIGUSR1 (SUSPEND)..."
kill -SIGUSR1 $COBALT_PID
sleep 5

log "Sending SIGCONT (RESUME)..."
kill -SIGCONT $COBALT_PID
sleep 5

log "Sending SIGUSR1 (SUSPEND) again..."
kill -SIGUSR1 $COBALT_PID
sleep 5

log "Sending SIGCONT (RESUME) again..."
kill -SIGCONT $COBALT_PID
sleep 10

log "Sending SIGPWR (STOP/QUIT)..."
kill -SIGPWR $COBALT_PID

# Wait for process to exit
wait $COBALT_PID
EXIT_CODE=$?

log "Cobalt exited with code: $EXIT_CODE"
log "Reviewing logs for fatal errors..."

if grep -q "FATAL" tasks/lifecycle_run.log; then
    echo -e "${RED}FAIL: Found FATAL errors in log.${NC}"
    grep "FATAL" tasks/lifecycle_run.log
    exit 1
elif grep -q "Check failed" tasks/lifecycle_run.log; then
    echo -e "${RED}FAIL: Found Check failures in log.${NC}"
    grep "Check failed" tasks/lifecycle_run.log
    exit 1
else
    echo -e "${GREEN}SUCCESS: No fatal errors or check failures found.${NC}"
fi

log "End of test."

# Example output of a passing test:
# [TEST] Starting cobalt_loader in background...
# [TEST] Launched PID: 1234567. Waiting for initialization (10s)...
# [TEST] Sending SIGUSR1 (SUSPEND)...
# [TEST] Sending SIGCONT (RESUME)...
# [TEST] Sending SIGUSR1 (SUSPEND) again...
# [TEST] Sending SIGCONT (RESUME) again...
# [TEST] Sending SIGPWR (STOP/QUIT)...
# [TEST] Cobalt exited with code: 0
# [TEST] Reviewing logs for fatal errors...
# SUCCESS: No fatal errors or check failures found.
# [TEST] End of test.
