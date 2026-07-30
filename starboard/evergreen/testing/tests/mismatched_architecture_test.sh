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

TEST_NAME="MismatchedArchitecture"
TEST_FILE="test.html"

function run_test() {
  clear_storage

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=tmsabi" "${TEST_NAME}.0.log" "update from tmsabi channel was installed"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to download and install the tmsabi package"
    return 1
  fi

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=tmsabi" "${TEST_NAME}.1.log" "Failed to load(ed)? ELF header"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to recognize architecture mismatch"
    return 1
  fi

  if ! grep -Eq "RevertBack current_installation=[0-9]+" "${LOG_PATH}/${TEST_NAME}.1.log"; then
    log "error" "Failed to revert after mismatched download"
    return 1
  fi

  return 0
}
