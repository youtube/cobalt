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

AUTH_METHOD="public-key"
USE_COMPRESSED_SYSTEM_IMAGE="false"
SYSTEM_IMAGE_EXTENSION=".so"

DISABLE_TESTS="false"

while getopts "d:a:c" o; do
    case "${o}" in
        d)
            DEVICE_ID=${OPTARG}
            ;;
        a)
            AUTH_METHOD=${OPTARG}
            ;;
        c)
            USE_COMPRESSED_SYSTEM_IMAGE="true"
            SYSTEM_IMAGE_EXTENSION=".lz4"
            ;;
    esac
done
shift $((OPTIND-1))

if [[ ! -f "${DIR}/setup.sh" ]]; then
  echo "The script 'setup.sh' is required"
  exit 1
fi

source $DIR/setup.sh

if [[ "${USE_COMPRESSED_SYSTEM_IMAGE}" == "true" ]]; then
  # It would be valid to run all test cases using a compressed system image but
  # is probably excessive. Instead, just two cases are run: one to test that a
  # compressed system image can be loaded and one to test that a compressed
  # update can be upgraded to.
  TESTS=($(eval "find ${DIR}/tests -maxdepth 1 \( -name 'evergreen_lite_test.sh' -o -name 'verify_qa_channel_compressed_update_test.sh' \)"))
else
  TESTS=($(eval "find ${DIR}/tests -maxdepth 1 -name '*_test.sh'"))
fi

COUNT=0
RETRIED=()
FAILED=()
PASSED=()
SKIPPED=()

log "info" " [==========] Deploying Cobalt."

deploy_cobalt "${DIR}/${1}"

log "info" " [==========] Running ${#TESTS[@]} tests."

# Loop over all of the tests found in the current directory and run them.
for test in "${TESTS[@]}"
do
  source $test "${DIR}/${1}"

  # Enable the tests to withstand some flakiness by allowing a small number
  # (i.e., 3) of test cases to be retried twice.
  if [[ ${#RETRIED[@]} -lt 3 ]]; then
    attempts_allowed=3
  else
    attempts_allowed=1
  fi

  for ((attempt=0; attempt<attempts_allowed; attempt++))
  do
    if [[ $attempt -gt 0 ]]; then
      RETRIED+=("$TEST_NAME")
    fi

    log "info" " [ RUN      ] ${TEST_NAME} attempt ${attempt}"


    if [[ "${DISABLE_TESTS}" == "true" ]]; then
      # Set the result to skipped.
      RESULT=2
    else
      run_test
      RESULT=$?
    fi

    stop_cobalt &> /dev/null

    if [[ "${RESULT}" -eq 0 ]]; then
      log "info"  " [   PASSED ] ${TEST_NAME} attempt ${attempt}"
      PASSED+=("${TEST_NAME}")
      break
    elif [[ "${RESULT}" -eq 1 ]]; then
      log "error" " [   FAILED ] ${TEST_NAME} attempt ${attempt}"
      if [[ $attempt -eq $(($attempts_allowed - 1)) ]]; then
        # No retry is available so consider the test case failed.
        FAILED+=("$TEST_NAME")
        break
      fi
    elif [[ "${RESULT}" -eq 2 ]]; then
      log "warning" " [  SKIPPED ] ${TEST_NAME} attempt ${attempt}"
      SKIPPED+=("$TEST_NAME")
      break
    fi
  done

  COUNT=$((COUNT + 1))
done

log "info" " [==========] ${COUNT} tests ran."

if [[ "${#RETRIED[@]}" -eq 1 ]]; then
  log "warning" " [  RETRIED ] 1 test, listed below:"
elif [[ "${#RETRIED[@]}" -gt 1 ]]; then
  log "warning" " [  RETRIED ] ${#RETRIED[@]} tests, listed below:"
fi

for test in "${RETRIED[@]}"; do
  log "warning" " [  RETRIED ] ${test}"
done

if [[ "${#PASSED[@]}" -eq 1 ]]; then
  log "info" " [  PASSED  ] 1 test."
elif [[ "${#PASSED[@]}" -gt 1 ]]; then
  log "info" " [  PASSED  ] ${#PASSED[@]} tests."
fi

if [[ "${#SKIPPED[@]}" -eq 1 ]]; then
  log "warning" " [  SKIPPED ] 1 test, listed below:"
elif [[ "${#SKIPPED[@]}" -gt 1 ]]; then
  log "warning" " [  SKIPPED ] ${#SKIPPED[@]} tests, listed below:"
fi

for test in "${SKIPPED[@]}"; do
  log "warning" " [  SKIPPED ] ${test}"
done

if [[ "${#FAILED[@]}" -eq 1 ]]; then
  log "error" " [  FAILED  ] 1 test, listed below:"
elif [[ "${#FAILED[@]}" -gt 1 ]]; then
  log "error" " [  FAILED  ] ${#FAILED[@]} tests, listed below:"
fi

for test in "${FAILED[@]}"; do
  log "error" " [  FAILED  ] ${test}"
done

log "info" " [==========] Cleaning up."

clean_up

log "info" " [==========] Finished testing with USE_COMPRESSED_SYSTEM_IMAGE=${USE_COMPRESSED_SYSTEM_IMAGE}."

if [[ "${#FAILED[@]}" -eq 0 ]]; then
  exit 0
fi

exit 1
