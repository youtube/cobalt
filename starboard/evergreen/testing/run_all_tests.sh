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
# Driver script that sets up the testing environment, collects all of the tests
# to run, runs them, and outputs the results.

DIR="$(dirname "${0}")"

if [[ ! -f "${DIR}/setup.sh" ]]; then
  echo "The script 'setup.sh' is required"
  exit 1
fi

source $DIR/setup.sh

# Find all of the test files within the 'test' subdirectory.
TESTS=($(eval "find ${DIR}/tests -maxdepth 1 -name '*_test.sh'"))

COUNT=0
PASSED=()
FAILED=()

info " [==========] Deploying Cobalt."

deploy_cobalt "${DIR}/${1}"

info " [==========] Running ${#TESTS[@]} tests."

# Loop over all of the tests found in the current directory and run them.
for test in "${TESTS[@]}"; do
  source $test "${DIR}/${1}"

  info " [ RUN      ] ${TEST_NAME}"

  run_test

  if [[ $? -eq 0 ]]; then
    info  " [   PASSED ] ${TEST_NAME}"
    PASSED+=("${TEST_NAME}")
  else
    error " [   FAILED ] ${TEST_NAME}"
    FAILED+=("$TEST_NAME")
  fi

  stop_cobalt &> /dev/null

  COUNT=$((COUNT + 1))
done

info " [==========] ${COUNT} tests ran."

# Output the number of passed tests.
if [[ "${#PASSED[@]}" -eq 1 ]]; then
  info " [  PASSED  ] 1 test."
elif [[ "${#PASSED[@]}" -gt 1 ]]; then
  info " [  PASSED  ] ${#PASSED[@]} tests."
fi

# Output the number of failed tests.
if [[ "${#FAILED[@]}" -eq 1 ]]; then
  error " [  FAILED  ] 1 test, listed below:"
elif [[ "${#FAILED[@]}" -gt 1 ]]; then
  error " [  FAILED  ] ${#FAILED[@]} tests, listed below:"
fi

# Output each of the failed tests.
for test in "${FAILED[@]}"; do
  error " [  FAILED  ] ${test}"
done

info " [==========] Cleaning up."

clean_up

info " [==========] Finished."

if [[ "${#FAILED[@]}" -eq 0 ]]; then
  exit 0
fi

exit 1
