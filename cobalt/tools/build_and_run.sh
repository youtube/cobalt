#!/bin/bash

# Exit immediately if any command fails, to prevent running a stale executable.
set -e

# --- Argument Parsing and Variable Setup ---

# Check if the user provided both platform and test suite name.
if [ "$#" -ne 2 ]; then
  echo "Error: Incorrect number of arguments."
  echo "Usage: $0 <platform> <test_suite_name>"
  echo "Example: $0 linux-x64x11_devel unittests"
  exit 1
fi

# Assign command-line arguments to variables for clarity.
PLATFORM="$1"
TEST_SUITE="$2"

# Derive the platform name for the filter path (e.g., "linux-x64x11_devel" -> "linux-x64x11").
# This removes the suffix starting with the last underscore.
FILTER_PLATFORM=${PLATFORM%_*}

# Construct paths based on the provided arguments.
BUILD_DIR="out/${PLATFORM}"
EXECUTABLE_PATH="./${BUILD_DIR}/${TEST_SUITE}"
FILTER_FILE="cobalt/testing/filters/${FILTER_PLATFORM}/${TEST_SUITE}_filter.json"

# --- Build Step ---
echo "🔨 Building ${TEST_SUITE} for ${PLATFORM}..."
autoninja -C "${BUILD_DIR}" "${TEST_SUITE}"

# --- Run Step ---
echo "🚀 Running ${TEST_SUITE}..."

GTEST_FILTER_FLAG=""

# Check if the filter file exists and if jq is installed.
if [ -f "$FILTER_FILE" ]; then
  if ! command -v jq &> /dev/null; then
    echo "⚠️ Warning: 'jq' is not installed. Cannot apply test filters. Running all tests."
  else
    echo "ℹ️ Found test filter file: ${FILTER_FILE}"
    # Use jq to extract failing tests, join them with a colon for the filter.
    FAILING_TESTS=$(jq -r '.failing_tests | join(":")' "$FILTER_FILE")
    if [ -n "$FAILING_TESTS" ]; then
      # Prepend '-' to exclude the failing tests.
      GTEST_FILTER_FLAG="--gtest_filter=-${FAILING_TESTS}"
      echo "🔬 Applying gtest_filter: ${GTEST_FILTER_FLAG}"
    else
      echo "ℹ️ No failing tests listed in filter file."
    fi
  fi
else
  echo "ℹ️ No filter file found for this test suite."
fi

# This part will only run if the build command above succeeds.
# The $GTEST_FILTER_FLAG will be empty if no filter is found or created.
echo "Executing: ${EXECUTABLE_PATH} --single-process-tests $GTEST_FILTER_FLAG"
"${EXECUTABLE_PATH}" --single-process-tests $GTEST_FILTER_FLAG

echo "✅ Done."