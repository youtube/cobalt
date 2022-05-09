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

# Runs an update cycle to generate performance logs.
#
# The size of the update is reported by this function directly, while memory
# usage and shared library load time are logged by the loader app process.
#
# Args:
#   Trial number, extra arguments for the loader app run to install an update,
#   extra arguments for the loader app run to load the update.
function run_update_trial() {
  clear_storage

  log "info" "Running trial ${1}"
  # Run the loader app once until an update has been installed.
  cycle_cobalt "https://www.youtube.com/tv" "${TEST_NAME}.0.log" "PingSender::SendPingComplete" "--loader_track_memory=100 ${2}"

  run_command "ls -l \"${STORAGE_DIR}/installation_1/lib\""

  # And run the loader app once more until it loads the installed update. The
  # memory tracker is configured with a shorter period to capture more data
  # during loading of the shared library.
  cycle_cobalt "https://www.youtube.com/tv" "${TEST_NAME}.1.log" "Loading took" "--loader_track_memory=10 ${3}"

  if grep -Eq "content/app/cobalt/lib/" "${LOG_PATH}/${TEST_NAME}.1.log"; then
    log "error" "The system image was loaded instead of the update, which must have failed"
    return 1
  fi

  return 0
}
