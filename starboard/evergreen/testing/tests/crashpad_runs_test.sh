#!/bin/bash
#
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

TEST_NAME="CrashpadRuns"
TEST_FILE="test.html"

function run_test() {
  clear_storage

  LOG="${TEST_NAME}.0.log"
  start_cobalt "file:///tests/${TEST_FILE}?channel=tcrash" "${LOG}" LOADER

  if [[ $(run_command "ps -C crashpad_handler") -ne 0 ]]; then
    log "error" " Failed to start crashpad_handler"
    return 1
  fi

  watch_cobalt "${LOG}" "update from tcrash channel was installed"

  if [[ $? -ne 0 ]]; then
    log "error" " Failed to download and install the tcrash package"
    return 1
  fi

  log "info" " Stopping with 'kill -9 ${LOADER}'"
  kill -9 "${LOADER}" 1> /dev/null
  sleep 1

  LOG="${TEST_NAME}.1.log"
  start_cobalt "file:///tests/${TEST_FILE}?channel=tcrash" "${LOG}" LOADER

  watch_cobalt "${LOG}" "Caught signal: SIG[A-Z]{4}"

  if [[ $? -ne 0 ]]; then
    log "error" " Binary did not crash"
    return 1
  fi

  if [[ $(run_command "find ${CACHE_DIR}/crashpad_database/completed/ -mmin -3 | ${WC} -l") -eq 0 ]]; then
    log "error" " Failed upload crash to crash database"
    return 1
  fi

  return 0
}
