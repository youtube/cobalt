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

TEST_NAME="OutOfStorage"
TEST_FILE="test.html"

function run_test() {
  if [[ ! -z "${CAN_MOUNT_TMPFS}" ]] && [[ "${CAN_MOUNT_TMPFS}" -eq 0 ]]; then
    echo " Cannot mount temporary filesystem, skipping"
    return 2
  fi

  clear_storage

  # We do not delete the storage directory to avoid removing the existing icu
  # tables. However, this means that we need to move it temporarily to create
  # our symbolic link to the temporary filesystem.
  if [[ -d "${STORAGE_DIR}" ]]; then
    run_command "mv \"${STORAGE_DIR}\" \"${STORAGE_DIR}.tmp\"" 1> /dev/null
  fi

  run_command "ln -s \"${STORAGE_DIR_TMPFS}\" \"${STORAGE_DIR}\"" 1> /dev/null

  # We need to explicitly clear the "storage", i.e. the temporary filesystem,
  # since previous runs might have artifacts leftover.
  run_command "rm -rf ${STORAGE_DIR}/*" 1> /dev/null

  OLD_TIMEOUT="${TIMEOUT}"
  TIMEOUT=300

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=test" "${TEST_NAME}.0.log" "Failed to update, log \"error\" code is 12"

  # Remove the symbolic link.
  run_command "rm -f ${STORAGE_DIR}" 1> /dev/null

  # Move the storage directory back to its original location.
  if [[ -d "${STORAGE_DIR}.tmp" ]]; then
    run_command "mv \"${STORAGE_DIR}.tmp\" \"${STORAGE_DIR}\"" 1> /dev/null
  fi

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to run out of storage"
    return 1
  fi

  TIMEOUT="${OLD_TIMEOUT}"

  return 0
}
