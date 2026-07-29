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
#
# A set of helper functions to output colored text using ANSI escape codes.

function pprint() {
  local arg="-e"
  if [[ "${OSTYPE}" = "*darwin*" ]]; then
    arg=""
  fi

  # This command uses ANSI escape codes to attempt to output colored text. For
  # more INFOrmation see https://en.wikipedia.org/wiki/ANSI_escape_code.
  echo "$arg" "\033[0;${1}m${2}\033[0m" >&2
}

function log() {
  if [[ $# -lt 2 ]]; then
    echo "log requires a level and message"
  else
    case "${1}" in
      # green
      "info")
        pprint 32 "${2}"
        ;;
      # yellow
      "warning")
        pprint 33 "${2}"
        ;;
      # red
      "error")
        pprint 31 "${2}"
        ;;
    esac
  fi
}
