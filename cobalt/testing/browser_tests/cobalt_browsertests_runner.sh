#!/bin/sh
# Helper script to run Cobalt browser tests on systems without Python.
# Handles process isolation (one process per test) and merges XML results.

set -e

SCRIPT_DIR=$(dirname "$0")
BINARY_PATH="$SCRIPT_DIR/cobalt_browsertests"

if [ ! -f "$BINARY_PATH" ]; then
  echo "Error: Binary not found at $BINARY_PATH."
  echo "Ensure the script is in the same directory as cobalt_browsertests (e.g. out/Debug/)."
  exit 1
fi

# Default Configuration
TOTAL_SHARDS=1
SHARD_INDEX=0
XML_OUTPUT_FILE=""
EXTRA_ARGS=""
FILTER_ARG=""

# Parse Arguments
for i in "$@"; do
  case $i in
    --gtest_output=xml:*)
      XML_OUTPUT_FILE="${i#*=xml:}"
      ;;
    --gtest_total_shards=*)
      TOTAL_SHARDS="${i#*=}"
      ;;
    --gtest_shard_index=*)
      SHARD_INDEX="${i#*=}"
      ;;
    --test-launcher-total-shards=*)
      TOTAL_SHARDS="${i#*=}"
      ;;
    --test-launcher-shard-index=*)
      SHARD_INDEX="${i#*=}"
      ;;
    --gtest_filter=*)
      # Use for listing, but don't pass to run loop (we use specific filter there)
      FILTER_ARG="$i"
      ;;
    *)
      # Preserve other arguments
      EXTRA_ARGS="$EXTRA_ARGS $i"
      ;;
  esac
done

# Fallback to Environment Variables for Sharding
if [ "$TOTAL_SHARDS" = "1" ] && [ -n "$GTEST_TOTAL_SHARDS" ]; then
  TOTAL_SHARDS="$GTEST_TOTAL_SHARDS"
fi
if [ "$SHARD_INDEX" = "0" ] && [ -n "$GTEST_SHARD_INDEX" ]; then
  SHARD_INDEX="$GTEST_SHARD_INDEX"
fi

echo "Running tests from: $BINARY_PATH"
if [ -n "$XML_OUTPUT_FILE" ]; then
  echo "Merging results to: $XML_OUTPUT_FILE"
  # Initialize the final XML file
  echo '<?xml version="1.0" encoding="UTF-8"?>' > "$XML_OUTPUT_FILE"
  echo '<testsuites tests="0" failures="0" disabled="0" errors="0" time="0" name="AllTests">' >> "$XML_OUTPUT_FILE"
fi

# 1. List all tests using the binary
# We rely on awk to parse the "Suite.\n  Test" format into "Suite.Test"
# We also filter out DISABLED_ tests
ALL_TESTS=$("$BINARY_PATH" --gtest_list_tests $FILTER_ARG $EXTRA_ARGS | awk '
  /^[^ ]/ { suite = $1 }
  /^  / {
    test = $1
    if (index(test, "DISABLED_") == 0) {
      print suite test
    }
  }
')

# 2. Filter tests for Sharding
TESTS_TO_RUN=""
COUNT=0
for TEST in $ALL_TESTS; do
  # Skip comments or empty lines
  if [ -z "$TEST" ]; then continue; fi

  # Check shard assignment: (COUNT % TOTAL_SHARDS) == SHARD_INDEX
  MOD=$((COUNT % TOTAL_SHARDS))
  if [ "$MOD" -eq "$SHARD_INDEX" ]; then
    TESTS_TO_RUN="$TESTS_TO_RUN $TEST"
  fi
  COUNT=$((COUNT + 1))
done

echo "Shard $SHARD_INDEX/$TOTAL_SHARDS: Running tests..."

PASSED_COUNT=0
FAILED_COUNT=0
TEMP_XML="temp_shard_${SHARD_INDEX}.xml"

# 3. Run Loop
for TEST in $TESTS_TO_RUN; do
  echo "[RUN] $TEST"

  # Construct flags
  RUN_FLAGS="--gtest_filter=$TEST --no-sandbox --single-process --no-zygote --ozone-platform=starboard $EXTRA_ARGS"

  if [ -n "$XML_OUTPUT_FILE" ]; then
    RUN_FLAGS="$RUN_FLAGS --gtest_output=xml:$TEMP_XML"
  fi

  # Run the test
  # We use 'set +e' to allow failure without exiting the script
  set +e
  $BINARY_PATH $RUN_FLAGS
  RET=$?
  set -e

  if [ $RET -eq 0 ]; then
    PASSED_COUNT=$((PASSED_COUNT + 1))
  else
    echo "[FAILED] $TEST (Exit Code: $RET)"
    FAILED_COUNT=$((FAILED_COUNT + 1))
  fi

  # 4. Merge XML Result
  if [ -n "$XML_OUTPUT_FILE" ] && [ -f "$TEMP_XML" ]; then
    # Strip the first 2 lines (<?xml...> and <testsuites...>) and last line (</testsuites>)
    # Append the rest (the <testsuite> block) to the output file
    sed '1,2d; $d' "$TEMP_XML" >> "$XML_OUTPUT_FILE"
    rm -f "$TEMP_XML"
  fi
done

# 5. Finalize XML
if [ -n "$XML_OUTPUT_FILE" ]; then
  echo '</testsuites>' >> "$XML_OUTPUT_FILE"

  TOTAL_RUN=$((PASSED_COUNT + FAILED_COUNT))

  # Update the counts in the header.
  # We use sed to replace the placeholder "0"s.
  # Using a temp file for sed portability (some versions don't support -i well).
  sed "s/tests=\"0\"/tests=\"$TOTAL_RUN\"/; s/failures=\"0\"/failures=\"$FAILED_COUNT\"/" "$XML_OUTPUT_FILE" > "${XML_OUTPUT_FILE}.tmp"
  mv "${XML_OUTPUT_FILE}.tmp" "$XML_OUTPUT_FILE"
fi

echo "================================================="
echo "Total: $((PASSED_COUNT + FAILED_COUNT)), Passed: $PASSED_COUNT, Failed: $FAILED_COUNT"

if [ $FAILED_COUNT -ne 0 ]; then
  exit 1
fi
exit 0
