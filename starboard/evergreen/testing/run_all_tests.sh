#!/bin/bash
#
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
#
# Driver script used to find all of the tests, run all of the tests, and output
# the results of the tests. This script, and all other scripts, assume the
# availability of "find", "grep", "ln", "mv", and "rm".

DIR="$(dirname "${0}")"
DEVICE_ID="${2}"

if [[ ! -f "${DIR}/setup.sh" ]]; then
  echo "The script 'setup.sh' is required"
  exit 1
fi

source $DIR/setup.sh

# Find all of the test files within the 'test' subdirectory.
TESTS=($(eval "find ${DIR}/tests -maxdepth 1 -name '*_test.sh'"))

COUNT=0
FAILED=()
PASSED=()
SKIPPED=()

log "info" " [==========] Deploying Cobalt."

deploy_cobalt "${DIR}/${1}"

log "info" " [==========] Running ${#TESTS[@]} tests."

# Loop over all of the tests found in the current directory and run them.
for test in "${TESTS[@]}"; do
  source $test "${DIR}/${1}"

  log "info" " [ RUN      ] ${TEST_NAME}"

  run_test

  RESULT=$?

  if [[ "${RESULT}" -eq 0 ]]; then
    log "info"  " [   PASSED ] ${TEST_NAME}"
    PASSED+=("${TEST_NAME}")
  elif [[ "${RESULT}" -eq 1 ]]; then
    log "error" " [   FAILED ] ${TEST_NAME}"
    FAILED+=("$TEST_NAME")
  elif [[ "${RESULT}" -eq 2 ]]; then
    log "warning" " [  SKIPPED ] ${TEST_NAME}"
    SKIPPED+=("$TEST_NAME")
  fi

  stop_cobalt &> /dev/null

  COUNT=$((COUNT + 1))
done

log "info" " [==========] ${COUNT} tests ran."

# Output the number of passed tests.
if [[ "${#PASSED[@]}" -eq 1 ]]; then
  log "info" " [  PASSED  ] 1 test."
elif [[ "${#PASSED[@]}" -gt 1 ]]; then
  log "info" " [  PASSED  ] ${#PASSED[@]} tests."
fi

# Output the number of skipped tests.
if [[ "${#SKIPPED[@]}" -eq 1 ]]; then
  log "warning" " [  SKIPPED ] 1 test, listed below:"
elif [[ "${#SKIPPED[@]}" -gt 1 ]]; then
  log "warning" " [  SKIPPED ] ${#SKIPPED[@]} tests, listed below:"
fi

# Output each of the skipped tests.
for test in "${SKIPPED[@]}"; do
  log "warning" " [  SKIPPED ] ${test}"
done

# Output the number of failed tests.
if [[ "${#FAILED[@]}" -eq 1 ]]; then
  log "error" " [  FAILED  ] 1 test, listed below:"
elif [[ "${#FAILED[@]}" -gt 1 ]]; then
  log "error" " [  FAILED  ] ${#FAILED[@]} tests, listed below:"
fi

# Output each of the failed tests.
for test in "${FAILED[@]}"; do
  log "error" " [  FAILED  ] ${test}"
done

log "info" " [==========] Cleaning up."

clean_up

log "info" " [==========] Finished."

if [[ "${#FAILED[@]}" -eq 0 ]]; then
  exit 0
fi

exit 1
