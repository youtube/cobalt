#!/bin/bash

# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

# --- Test Analysis Script ---
#
# This script runs the log analyzer on a set of test log files and
# compares the output to expected results to prevent regressions.
#
# It should be run from the root of the repository.

set -e  # Exit immediately if a command exits with a non-zero status.

# --- Helper Functions ---
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

info() {
  echo -e "${GREEN}[INFO]${NC} $1"
}

error() {
  echo -e "${RED}[ERROR]${NC} $1" >&2
}

# --- Main Test Logic ---
TEST_DIR="agents/extensions/cobalt_builds/tests"
DO_SCRIPT="agents/extensions/cobalt_builds/do"

declare -a test_files=(
  "run_nplb_example1.log"
  "run_nplb_example2.log"
  "build_test_log.log"
  "run_nplb_long_test.log"
  "build_test_log_nonexistent.log"
  "build_nplb_unresolved_dependencies.log"
)

all_passed=true

for file in "${test_files[@]}"; do
  info "Testing analysis of ${file}..."
  full_path="${TEST_DIR}/${file}"
  expected_file="${full_path}.expected"
  temp_output_file="${TEST_DIR}/${file}.test_output"
  stderr_log_file="${TEST_DIR}/${file}.stderr.log"

  if [ ! -f "${expected_file}" ]; then
    error "Expected output file not found: ${expected_file}"
    all_passed=false
    continue
  fi

  # Run the analyzer and capture the output
  echo "Running analyzer..."
  if ./"${DO_SCRIPT}" analyze "${full_path}" > "${temp_output_file}" 2> "${stderr_log_file}"; then
    echo "Analyzer finished."
  else
    exit_code=$?
    error "Analyzer script failed for ${file} with exit code ${exit_code}."
    if [ -s "${stderr_log_file}" ]; then
        echo "--- STDERR ---"
        cat "${stderr_log_file}"
        echo "--------------"
    fi
    all_passed=false
    continue
  fi

  # Compare the output with the expected result
  echo "Comparing output..."
  if diff_output=$(diff "${temp_output_file}" "${expected_file}"); then
    info "SUCCESS: Output for ${file} matches expected."
    rm "${temp_output_file}"
  else
    error "FAILURE: Output for ${file} does not match expected."
    echo "The difference between the current output and the expected output is:"
    echo "NOTE: This is a diff from the current output to the expectyed output."
    echo "NOTE: Lines removed in this diff should NOT BE in the analyze output."
    echo "NOTE: Lines added in this diff are expected but missing from the analyze output."
    echo "${diff_output}"
    all_passed=false
  fi

  if [ ! -s "${stderr_log_file}" ]; then
    rm "${stderr_log_file}"
  fi
done

# --- Final Status ---
if ${all_passed}; then
  info "All analysis tests passed!"
  exit 0
else
  error "One or more analysis tests failed."
  exit 1
fi
