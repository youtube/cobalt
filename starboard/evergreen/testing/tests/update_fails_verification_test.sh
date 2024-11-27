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

TEST_NAME="UpdateFailsVerification"
TEST_FILE="test.html"

function run_test() {
  clear_storage

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=tfailv" "${TEST_NAME}.0.log" "Verification failed. Verifier error = [0-9]+"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to fail verifying the downloaded installation"
    return 1
  fi

  cycle_cobalt "file:///tests/${TEST_FILE}?channel=tfailv" "${TEST_NAME}.1.log" "App is up to date"

  if [[ $? -ne 0 ]]; then
    log "error" "Failed to revert back to the system image"
    return 1
  fi

  return 0
}
