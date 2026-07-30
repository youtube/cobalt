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

# Searches the provided log file for the path to the bad app key file.
#
# Globals:
#   LOG_PATH
#   TAIL
#
# Args:
#   Path to a log file.
#
# Returns:
#   Path to the bad app key file, otherwise "".
function get_bad_app_key_file_path() {
  if [[ $# -ne 1 ]]; then
    log "error" " get_bad_app_key_file_path only accepts a single argument"
    return 1
  fi

  # Warning: do not wrap '$TAIL' with double quotes or else it will not actually
  # resolve to the correct command.
  echo "$( \
    sed -n "s/.*bad_app_key_file_path: \(.*app_key_[A-Za-z0-9+/=]\{32,\}\.bad\).*/\1/p" \
      "${LOG_PATH}/${1}" | ${TAIL} -1)"
}
