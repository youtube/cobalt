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

# Watches Cobalt's logs.
#
# Globals:
#   CAT_OUTPUT
#   LOG_PATH
#   TIMEOUT
#
# Args:
#   path for logging, pattern to search logs for.
#
# Returns:
#   0 if the provided pattern was found in the logs, otherwise 1.
function watch_cobalt() {
  if [[ $# -lt 2 ]]; then
    log "error" " watch_cobalt missing args"
    return 1
  fi

  LOG="${1}"
  PAT="${2}"

  log "info" " Waiting up to ${TIMEOUT} seconds for \"${PAT}\" to be logged"

  wait_and_watch "${PAT}" "${LOG_PATH}/${LOG}"

  FOUND=$?

  # For platforms without the `tee` command.
  if [[ "${CAT_OUTPUT}" -eq 1 ]]; then
    cat "${LOG_PATH}/${LOG}"
  fi

  log "info" " Finished after ${WAITED} seconds"

  if [[ "${FOUND}" -eq 1 ]]; then
    return 0
  fi

  return 1
}

# Cycles (runs then kills) Cobalt on the desired platform.
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
function cycle_cobalt() {
  if [[ $# -lt 3 ]]; then
    log "error" " cycle_cobalt missing args"
    return 1
  fi

  URL="${1}"
  LOG="${2}"
  PAT="${3}"
  ARGS="${4}"

  start_cobalt "${URL}" "${LOG}" LOADER "${ARGS}"

  watch_cobalt "${LOG}" "${PAT}"
  RES=$?

  log "info" " Stopping ${LOADER}"

  stop_process "${LOADER}"

  sleep 1

  return $RES
}
