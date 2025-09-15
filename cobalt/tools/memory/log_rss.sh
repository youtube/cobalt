#!/bin/bash
#
# log_rss.sh - Runs a command in a specified directory, logs its RSS memory
#              for a fixed duration, and repeats the process multiple times.
#
# Usage: ./log_rss.sh [path/to/directory]
#

RUN_COUNT=10
DURATION=60 # seconds
INTERVAL=1  # seconds
COMMAND_TO_RUN="./elf_loader_sandbox --evergreen_library=libcobalt.so --evergreen_content=."
# Use a unique part of the command to avoid killing other processes
COMMAND_PATTERN="elf_loader_sandbox --evergreen_library=libcobalt.so"
TARGET_DIR=${1:-.} # Default to current directory if not provided
OUTPUT_FILE="$(pwd)/rss_log.csv" # Use absolute path for output

echo "Starting memory analysis for command: \"$COMMAND_TO_RUN\""
echo "Target Directory: $TARGET_DIR"
echo "Number of runs: $RUN_COUNT"
echo "Duration per run: $DURATION seconds"
echo "Output will be saved to $OUTPUT_FILE"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' not found."
    exit 1
fi

# Write CSV header
echo "run,elapsed_seconds,rss_kb" > "$OUTPUT_FILE"

for (( run=1; run<=RUN_COUNT; run++ )); do
  echo ""
  echo "--- Starting Run $run of $RUN_COUNT ---"

  # Change to the target directory
  pushd "$TARGET_DIR" > /dev/null

  # Start the command in the background
  eval "$COMMAND_TO_RUN" &
  PID=$!
  echo "Process started with PID: $PID"

  # Allow a moment for the process to initialize
  sleep 2

  # Find the actual process PID to monitor, as the initial one might be a wrapper
  MONITOR_PID=$(pgrep -f "$COMMAND_PATTERN")
  if [ -z "$MONITOR_PID" ]; then
      echo "Error: Could not find the main process. It may have crashed on start."
      popd > /dev/null
      continue
  fi
  echo "Monitoring actual process PID: $MONITOR_PID"


  echo "Logging RSS for $DURATION seconds..."
  for (( elapsed=0; elapsed<DURATION; elapsed++ )); do
    if ! ps -p $MONITOR_PID > /dev/null; then
      echo "Process $MONITOR_PID exited prematurely."
      break
    fi
    RSS=$(ps -p $MONITOR_PID -o rss= | tr -d ' ')
    echo "$run,$elapsed,$RSS" >> "$OUTPUT_FILE"
    sleep $INTERVAL
  done

  echo "Stopping Cobalt process..."
  # Use pkill to find and terminate the process by its command line.
  pkill -TERM -f "$COMMAND_PATTERN"
  sleep 2 # Give it time to clean up

  # Verify and force kill if it's still running
  if pgrep -f "$COMMAND_PATTERN" > /dev/null; then
    echo "Process did not terminate gracefully, sending KILL signal."
    pkill -KILL -f "$COMMAND_PATTERN"
  fi

  # Return to the original directory
  popd > /dev/null

  echo "--- Run $run Finished ---"
done

echo ""
echo "All runs complete. Data saved in $OUTPUT_FILE."
