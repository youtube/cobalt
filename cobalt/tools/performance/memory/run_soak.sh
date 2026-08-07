#!/bin/bash
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Exit immediately if a command exits with a non-zero status.
set -e

PORT=${PORT:-9222}
DURATION=${DURATION:-21600}  # 6 hours
INTERVAL=${INTERVAL:-300}    # 5 minutes
TARGET_URL="https://youtube.com/tv"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_DIR="/usr/local/google/home/ahmedelzeiny/soak_results_${TIMESTAMP}"

# Start virtual display Xvfb
XVFB_DISPLAY=":99"
echo "🖥️ Starting Xvfb on display $XVFB_DISPLAY..."
Xvfb $XVFB_DISPLAY -screen 0 1920x1080x24 > /dev/null 2>&1 &
XVFB_PID=$!
sleep 2 # Let Xvfb initialize
export DISPLAY=$XVFB_DISPLAY

echo "=========================================================="
echo "          COBALT MEMORY SOAK TEST ORCHESTRATOR            "
echo "=========================================================="
echo "Target URL:  $TARGET_URL"
echo "Duration:    $DURATION seconds (~$(expr $DURATION / 3600) hours)"
echo "Interval:    $INTERVAL seconds"
echo "Display:     $DISPLAY"
echo "Output Dir:  $OUTPUT_DIR"
echo "=========================================================="

mkdir -p "$OUTPUT_DIR"

# Ensure no existing cobalt or loader_app is running
if pidof cobalt >/dev/null || pidof loader_app >/dev/null; then
  echo "⚠️ Warning: Cobalt/loader_app is already running. Stopping running instances..."
  killall -9 cobalt loader_app || true
  sleep 2
fi

echo "🚀 Launching Local Cobalt Devel Binary..."
./out/linux-x64x11_devel/cobalt \
  --remote-debugging-port=${PORT} \
  --remote-allow-origins=* \
  --enable-heap-profiling \
  --enable-features=CobaltMemoryAttributionManager \
  --url="${TARGET_URL}" > "${OUTPUT_DIR}/cobalt_run.log" 2>&1 &

LAUNCHER_PID=$!

# Setup cleanup trap to kill Cobalt and Launcher when script exits
cleanup() {
  echo "🧹 Cleaning up background processes..."
  if [ ! -z "$COBALT_PID" ] && kill -0 $COBALT_PID 2>/dev/null; then
    echo "Killing Cobalt (PID: $COBALT_PID)..."
    kill $COBALT_PID || kill -9 $COBALT_PID
  fi
  if [ ! -z "$LAUNCHER_PID" ] && kill -0 $LAUNCHER_PID 2>/dev/null; then
    kill $LAUNCHER_PID || kill -9 $LAUNCHER_PID
  fi
  if [ ! -z "$XVFB_PID" ] && kill -0 $XVFB_PID 2>/dev/null; then
    echo "Stopping Xvfb (PID: $XVFB_PID)..."
    kill $XVFB_PID || kill -9 $XVFB_PID
  fi
  echo "Done."
}
trap cleanup EXIT INT TERM

# Wait for Cobalt DevTools to become active
echo "Waiting for DevTools endpoint to become active on port ${PORT}..."
DEVTOOLS_ACTIVE=false
for i in {1..180}; do
  if curl -s "http://localhost:${PORT}/json/version" >/dev/null; then
    echo "✅ DevTools is active!"
    DEVTOOLS_ACTIVE=true
    break
  fi
  sleep 1
done

if [ "$DEVTOOLS_ACTIVE" = false ]; then
  echo "❌ Error: DevTools did not become active in 180 seconds! Check logs at ${OUTPUT_DIR}/cobalt_run.log"
  exit 1
fi

COBALT_PID=$(pgrep -x loader_app | head -n 1)
if [ -z "$COBALT_PID" ]; then
  COBALT_PID=$(pgrep -x cobalt | head -n 1)
fi

if [ -z "$COBALT_PID" ]; then
  echo "❌ Error: Could not find Cobalt or loader_app PID!"
  exit 1
fi
echo "Cobalt/loader_app running with PID: $COBALT_PID"

# Trigger navigation with exit protection
python3 /usr/local/google/home/ahmedelzeiny/.gemini/jetski/brain/692ac953-852a-4391-add3-605cb8455f32/scratch/test_navigate_protected.py &

# Run telemetry collector
echo "🔄 Starting telemetry collection..."
python3 cobalt/tools/performance/memory/soak_collector.py \
  --pid $COBALT_PID \
  --port $PORT \
  --interval_seconds $INTERVAL \
  --duration_seconds $DURATION \
  --output_dir "$OUTPUT_DIR"

echo "🎉 Soak test run successfully completed!"
