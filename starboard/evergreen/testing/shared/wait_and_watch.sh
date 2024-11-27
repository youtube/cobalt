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

# Polls a file for the provided pattern.
#
# Globals:
#   TIMEOUT
#   WAITED
#
# Args:
#   A pattern to check for, a path to a file to check.
#
# Returns:
#   1 if the pattern was found, otherwise 0.
function wait_and_watch() {
  if [[ $# -ne 2 ]]; then
    log "error" " wait_and_watch requires a pattern and a path"
    return 1
  fi

  WAITED=0

  while [[ "${WAITED}" -lt "${TIMEOUT}" ]]; do
    if grep -Eq "${1}" "${2}"; then
      return 1
    fi

    WAITED=$((WAITED + 1))
    sleep 1
  done

  return 0
}
