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

TEST_NAME="NoOpBinary"
TEST_FILE="test.html"

function run_test() {
  clear_storage

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=tnoop" "${TEST_NAME}.0.log" "update from tnoop channel was installed"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to download and install the tnoop package"
    return 1
  fi

  for i in {1..3}; do
    cycle_cobalt "file:///tests/${TEST_FILE}?channel=tnoop" "${TEST_NAME}.${i}.log" "Load start=0x[a-f0-9]{8,} base_memory_address=0x[a-f0-9]{8,}"

    if [[ $? -ne 0 ]]; then
      log "error" "Failed to load binary"
      return 1
    fi

    if grep -Eq "Starting application." "${LOG_PATH}/${TEST_NAME}.${i}.log"; then
      log "error" "Failed to run no-op binary"
      return 1
    fi
  done

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=tnoop" "${TEST_NAME}.4.log" "App is up to date"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to revert to working installation after no-oping 3 times"
    return 1
  fi

  return 0
}
