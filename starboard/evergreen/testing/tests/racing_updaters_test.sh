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

# Unset the previous test's name and runner function.
unset TEST_NAME
unset TEST_FILE
unset -f run_test

TEST_NAME="RacingUpdaters"
TEST_FILE="test.html"

function wait_and_force_race_condition() {
  if [[ $# -ne 3 ]]; then
    log "error" " wait_and_force_race_condition requires a pattern, a path and a filename"
    return 1
  fi

  # The previous logs are available when this is invoked. Wait before beginning
  # to check the logs.
  sleep 15

  wait_and_watch "${1}" "${2}"

  FOUND=$?

  if [[ "${FOUND}" -eq 1 ]]; then
    create_file "${3}"
  fi
}

function run_test() {
  clear_storage

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=test" "${TEST_NAME}.0.log" "Created drain file at"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to create a drain file for the test package"
    return 1
  fi

  FILENAME="$(get_temporary_drain_file_path "${TEST_NAME}.0.log")"

  if [[ -z "${FILENAME}" ]]; then
    log "error" "Failed to evaluate a temporary drain file path"
    return 1
  fi

  clear_storage

  wait_and_force_race_condition "Created drain file at" "${LOG_PATH}/${TEST_NAME}.1.log" "${FILENAME}" &

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=test" "${TEST_NAME}.1.log" "failed to lock slot"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to recognize another update is updating slot"
    return 1
  fi

  return 0
}
