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

# Creates a path to a valid drain file based on the drain file created in the
# provided log file.
#
# Globals:
#   LOG_PATH
#   TAIL
#
# Args:
#   Path to a log file.
#
# Returns:
#   Path to a valid drain file, otherwise "".
function get_temporary_drain_file_path() {
  if [[ $# -ne 1 ]]; then
    log "error" " get_temporary_drain_file_path only accepts a single argument"
    return 1
  fi

  # Warning: do not wrap '$TAIL' with double quotes or else it will not actually
  # resolve to the correct command.
  echo "$( \
    sed -n "s/.*Created drain file at '\(.*d_\)[A-Za-z0-9+/=]\{32,\}\(_[0-9]\{8,\}\).*/\1TEST\2/p" \
      "${LOG_PATH}/${1}" | ${TAIL} -1)"
}
