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

TEST_NAME="CompressionUpdatePerformance"

function run_test() {
  source $(dirname "$0")/performance_tests/run_update_trial.sh

  for i in {1..3}; do
    run_update_trial "$i" "--compress_update" "--loader_use_compression";
    result=$?
    if [[ "${result}" -ne 0 ]]; then
      return 1
    fi
  done

  return 0
}
