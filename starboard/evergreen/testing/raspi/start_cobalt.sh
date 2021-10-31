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
  declare -n loader_pid_ref=$3
  ARGS="${4}"

  stop_cobalt

  log "info" " Starting Cobalt with:"
  log "info" "  --url=${URL}"

  for arg in $ARGS; do
    log "info" "  ${arg}"
  done

  log "info" " Logs will be output to '${LOG_PATH}/${LOG}'"

  # A file name unique to the process, provided by the current time in
  # milliseconds, isn't strictly needed but is used out of caution.
  declare pid_storage_path="${PID_STORAGE_DIR}/$(date +%s%3N)"

  # The stored process ID initially points to a remote shell process but that
  # process image is quickly replaced, via exec, with a new one executing the
  # loader app. This approach enables the local machine to obtain the loader
  # app's process ID without interfering with its stdout/stderr, which must be
  # piped to tee.
  declare store_pid_cmd="echo \\$\\$ > ${pid_storage_path}"
  declare replace_with_loader_cmd="exec /home/pi/coeg/loader_app --url=\"\"${URL}\"\" ${ARGS}"
  eval "${SSH}\"${store_pid_cmd};${replace_with_loader_cmd}\" 2>&1 | tee \"${LOG_PATH}/${LOG}\"" &

  # The device's filesystem is polled to avoid a race condition since the
  # previous eval command is necessarily run in the background.
  eval "${SSH}\"while [[ ! -f ${pid_storage_path} ]] ; do sleep 1 ; done\""

  loader_pid_ref=$(eval "${SSH}\"cat ${pid_storage_path}\"")

  delete_file "${pid_storage_path}"

  log "info" " Cobalt process ID is ${loader_pid_ref}"
}
