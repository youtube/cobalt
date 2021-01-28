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

TIMEOUT=150

# Runs Cobalt on the desired platform.
#
# Globals:
#   CONTENT
#   LOG_PATH
#   TIMEOUT
#
# Args:
#   URL, path for logging, pattern to search logs for, extra arguments.
#
# Returns:
#   0 if the provided pattern was found in the logs, otherwise 1.
function start_cobalt() {
  if [[ $# -lt 3 ]]; then
    log "error" " start_cobalt missing args"
    return 1
  fi

  URL="${1}"
  LOG="${2}"
  PAT="${3}"
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

  LOADER=$!

  log "info" " Cobalt process ID is ${LOADER}"
  log "info" " Waiting up to ${TIMEOUT} seconds for \"${PAT}\" to be logged"

  wait_and_watch "${PAT}" "${LOG_PATH}/${LOG}"

  FOUND=$?

  log "info" " Finished after ${WAITED} seconds"
  log "info" " Stopping with 'kill -9 ${LOADER}'"

  kill -9 "${LOADER}" &> /dev/null

  sleep 1

  if [[ "${FOUND}" -eq 1 ]]; then
    return 0
  fi

  return 1
}
