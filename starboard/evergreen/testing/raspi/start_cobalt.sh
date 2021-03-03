#!/bin/bash
#
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

# Runs Cobalt on the desired platform.
#
# Globals:
#   CONTENT
#   LOG_PATH
#
# Args:
#   URL, path for logging, variable to store Cobalt's pid in, extra arguments.
function start_cobalt() {
  if [[ $# -lt 3 ]]; then
    log "error" " start_cobalt missing args"
    return 1
  fi

  URL="${1}"
  LOG="${2}"
  __LOADER="${3}"
  ARGS="${4}"

  stop_cobalt

  log "info" " Starting Cobalt with:"
  log "info" "  --url=${URL}"
  log "info" "  --content=${CONTENT}"

  for arg in $ARGS; do
    log "info" "  ${arg}"
  done

  log "info" " Logs will be output to '${LOG_PATH}/${LOG}'"

  eval "${SSH}\"/home/pi/coeg/loader_app --url=\"\"${URL}\"\" --content=${CONTENT} ${ARGS} \" 2>&1 | tee \"${LOG_PATH}/${LOG}\"" &

  eval $__LOADER=$!

  log "info" " Cobalt process ID is ${__LOADER}"
}
