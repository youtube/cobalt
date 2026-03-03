#!/bin/bash

# --- Cobalt Control Script ---
# This script manages a debug session:
# 1. Stops any running instance of the Cobalt application.
# 2. Clears old logcat logs.
# 3. Starts the Perffetto trace if enabled.
# 4. Starts the Cobalt application using Blaze and waits for it to exit.
# 5. Kick off a loop to collect CPU and memory usage every second for 60 seconds, saving it to a CSV file.
# 6. Dumps the logcat output to a file.
# 7. Checks the log for a crash and processes the tombstone if one is found.

# --- CONFIGURATION ---
# IMPORTANT: You MUST set the device serial number here.
DEVICE_SERIAL="GZ2306036N390231"

# IMPORTANT: Set the path to your build output directory.
# This is needed to find the 'stack' symbolization tool.
# Example: BUILD_DIR="out2/android_arm_dbg"
BUILD_DIR="out/android-arm_gold"

# The Android package name for the Cobalt application.
# PACKAGE_NAME="dev.cobalt.coat"
PACKAGE_NAME="com.google.android.youtube.tv"

# The local file where the device log will be saved.
LOG_FILE="out/cobalt_session.log"

BUILD_ROOT="~/cobalt/src"

# Parse command line arguments
ENABLE_PERFETTO=false
while getopts ":p" opt; do
  case ${opt} in
    p)
      ENABLE_PERFETTO=true
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))

# Validate Configuration
if [ -z "$DEVICE_SERIAL" ]; then
  echo "Error: DEVICE_SERIAL is not set. Please edit this script and provide your device's serial number."
  adb devices
  exit 1
fi

echo "Targeting device: $DEVICE_SERIAL"
export ANDROID_SERIAL=$DEVICE_SERIAL

# Stop Lingering App Process
echo "Attempting to stop any running instance of '$PACKAGE_NAME'..."
adb -s "$DEVICE_SERIAL" shell am force-stop "$PACKAGE_NAME"
echo "Stop command sent."

# Clear Device Logs
echo "Clearing logcat buffer..."
adb -s "$DEVICE_SERIAL" logcat -c

# Run the Application
echo ""
echo "--- Starting Cobalt Application ---"

# If Perfetto tracing is enabled, start it in the background
if [ "$ENABLE_PERFETTO" = true ]; then
  echo "Starting Perfetto trace..."
  rm -f out/perfetto.log

  # Clean up memory cache in the device. Root required.
  adb -s "$DEVICE_SERIAL" shell "echo 3 > /proc/sys/vm/drop_caches"
  # (Optional) Compact memory to start from a clean slate
  adb -s "$DEVICE_SERIAL" shell "echo 1 > /proc/sys/vm/compact_node"

  export PYTHONUNBUFFERED=1
  third_party/perfetto/tools/record_android_trace -n -t 80s -c config.pbtx -o out/trace.pftrace > out/perfetto.log 2>&1 &
  PERFETTO_PID=$!

  echo "Waiting for Perfetto trace to start..."
  # Wait for "Trace started. Press CTRL+C to stop" to appear in the log
  tail -f -n +1 out/perfetto.log | grep -q "Trace started. Press CTRL+C to stop"
  echo "Perfetto trace started."
fi

echo "Start application"
adb -s "$DEVICE_SERIAL" shell am start -a android.intent.action.VIEW -d "https://www.youtube.com/watch?v=LXb3EKWsInQ" --esa commandLineArgs "--remote-allow-origins=*" "$PACKAGE_NAME"

# Dump the Log
echo "Waiting 360 seconds before dumping logcat to '$LOG_FILE'..."

DURATION=60
INTERVAL=1
END_TIME=$(( SECONDS + DURATION ))
CSV_FILE="out/perf_log_${PACKAGE_NAME}_$(date +%Y%m%d_%H%M%S).csv"
while [ $SECONDS -lt $END_TIME ]; do
  TIMESTAMP=$(date +"%H:%M:%S")

  CPU_INFO=$(adb -s "$DEVICE_SERIAL" shell top -b -n 1 | grep "$PACKAGE_NAME" | awk '{sum+=$9} END {print sum}')
  MEM_INFO=$(adb -s "$DEVICE_SERIAL" shell dumpsys meminfo "$PACKAGE_NAME" | grep -w "TOTAL" | head -n 1 | awk '{print $2}')

  if [ -z "$CPU_INFO" ]; then CPU_INFO="0"; fi
  [ -z "$MEM_INFO" ] && MEM_INFO="0"

  echo "$TIMESTAMP,$CPU_INFO%,$MEM_INFO" >> "$CSV_FILE"
  echo "[$TIMESTAMP] CPU: $CPU_INFO% | Mem: $MEM_INFO KB"

  sleep $INTERVAL
done

adb -s "$DEVICE_SERIAL" logcat -d > "$LOG_FILE"
echo "Logcat dump complete. Log saved to '$LOG_FILE'."

# Check for Crash and Process Tombstone

# Find the latest tombstone file
echo "Finding the latest tombstone in /data/tombstones/..."
LATEST_TOMBSTONE=$(adb -s "$DEVICE_SERIAL" shell 'ls -lt /data/tombstones/ | head -n 2 | tail -n 1' | awk '{print $NF}')

if [ -z "$LATEST_TOMBSTONE" ]; then
  echo "Error: Crash was detected, but no tombstone files were found in /data/tombstones/."
  exit 1
fi

# echo "Latest tombstone found: $LATEST_TOMBSTONE"
TOMBSTONE_PATH="/data/tombstones/$LATEST_TOMBSTONE"

# Pull the tombstone file
echo "Pulling $LATEST_TOMBSTONE to the current directory..."
adb -s "$DEVICE_SERIAL" pull "$TOMBSTONE_PATH" out2/tombstone
"$BUILD_ROOT"/third_party/android_platform/development/scripts/stack \
  --output-directory "$BUILD_ROOT"/"$BUILD_DIR" \
  "$BUILD_ROOT"/out2/tombstone > "$BUILD_ROOT"/out2/stack01.log 2>&1


adb -s "$DEVICE_SERIAL" shell am force-stop "$PACKAGE_NAME"

if [ "$ENABLE_PERFETTO" = true ]; then
  echo "Waiting for Perfetto trace to complete..."
  wait $PERFETTO_PID
  if grep -q "file pulled" out/perfetto.log; then
      echo "Perfetto trace capture confirmed."
      cp out/trace.pftrace /google/src/cloud/linxinan/kimono/google3/experimental/users/linxinan/trace/
  else
      echo "Warning: Did not see 'file pulled' message in perfetto log."
  fi
fi
