#!/bin/bash

# Exit immediately if any command fails, to prevent running a stale executable.
set -e

# --- Argument Parsing and Variable Setup ---

# Check if the user provided both platform and test suite name.
if [ "$#" -ne 2 ]; then
  echo "Error: Incorrect number of arguments."
  echo "Usage: $0 <platform> <test_suite_name>"
  echo "Example: $0 linux-x64x11_devel unittests"
  echo "Example: $0 evergreen-x64_devel net_unittests_loader" # Added example
  exit 1
fi

# Assign command-line arguments to variables for clarity.
PLATFORM="$1"
TEST_SUITE="$2"

# Derive the platform name for the filter path (e.g., "linux-x64x11_devel" -> "linux-x64x11").
# This removes the suffix starting with the last underscore.
FILTER_PLATFORM=${PLATFORM%_*}
BUILD_DIR="out/${PLATFORM}"
FILTER_FILE="cobalt/testing/filters/${FILTER_PLATFORM}/${TEST_SUITE}_filter.json"


# Check if the TEST_SUITE string ends with "_loader"
if [[ "$TEST_SUITE" == *_loader ]]; then
  # Construct the Python script path
  EXECUTABLE_PATH="./${BUILD_DIR}/${TEST_SUITE}.py"
else
  # Construct the C++ executable path
  EXECUTABLE_PATH="./${BUILD_DIR}/${TEST_SUITE}"
fi

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

# Check if the C++ executable exists before trying to run it
if [ -f "$EXECUTABLE_PATH" ]; then
  echo "Executing C++ test: ${EXECUTABLE_PATH} --single-process-tests $GTEST_FILTER_FLAG"
  "${EXECUTABLE_PATH}" --single-process-tests $GTEST_FILTER_FLAG
else
    echo "❌ Error: test executable not found at ${EXECUTABLE_PATH}"
    exit 1
fi

echo "✅ Done."