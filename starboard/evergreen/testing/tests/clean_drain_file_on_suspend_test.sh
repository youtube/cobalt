#!/bin/bash
#
# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

# Unset the previous test's name and runner function.
unset TEST_NAME
unset TEST_FILE
unset -f run_test

TEST_NAME="CleanDrainFilesOnSuspend"
TEST_FILE="test.html"

function run_test() {
  clear_storage

  LOG="${TEST_NAME}.0.log"
  start_cobalt "file:///tests/${TEST_FILE}?channel=test" "${LOG}" LOADER --update_check_delay_seconds=1

  VAR=1

  for i in {1..5}
  do
    log "info" "${TEST_NAME} #${VAR}"
    sleep ${VAR}

    # suspend
    kill -s USR1 "${LOADER}"
    if [[ $? -ne 0 ]]; then
      log "warning" "Failed to suspend"
      break
    fi

    sleep 5

    if ls ${STORAGE_DIR}/installation_?/d_* 1> /dev/null 2>&1; then
      echo "drain file wasn't cleaned up ."
      return 1
    fi

    # resume
    kill -s CONT "${LOADER}"
    if [[ $? -ne 0 ]]; then
      log "warning" "Failed to resume"
      break
    fi

    let "VAR=${VAR}+1"
  done
  return 0

}
