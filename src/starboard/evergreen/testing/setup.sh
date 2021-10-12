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
# Driver script used to perform the setup required for all of the tests.
# Platform specific setup is delegated to the platform specific setup scripts.

if [[ ! -f "${DIR}/pprint.sh" ]]; then
  echo "The script 'pprint.sh' is required"
  exit 1
fi

source $DIR/pprint.sh

log "info" " [==========] Preparing Cobalt."

if [[ $# -lt 1 ]]; then
  log "error" "A platform must be provided"
  exit 1
fi

PLATFORMS=("linux" "raspi")

if [[ ! "${PLATFORMS[@]}" =~ "${1}" ]] && [[ ! -d "${DIR}/${1}" ]]; then
  log "error" "The platform provided must be one of the following: ${PLATFORMS[*]}"
  exit 1
fi

# List of all required scripts.
SCRIPTS=("${DIR}/shared/app_key.sh"           \
         "${DIR}/shared/cycle_cobalt.sh"        \
         "${DIR}/shared/drain_file.sh"        \
         "${DIR}/shared/init_logging.sh"      \
         "${DIR}/shared/installation_slot.sh" \
         "${DIR}/shared/wait_and_watch.sh"    \

         # Each of the following scripts must be provided for the targeted
         # platform. The script 'setup.sh' must be source'd first.
         "${DIR}/${1}/setup.sh"               \

         "${DIR}/${1}/clean_up.sh"            \
         "${DIR}/${1}/clear_storage.sh"       \
         "${DIR}/${1}/create_file.sh"         \
         "${DIR}/${1}/delete_file.sh"         \
         "${DIR}/${1}/deploy_cobalt.sh"       \
         "${DIR}/${1}/run_command.sh"         \
         "${DIR}/${1}/start_cobalt.sh"        \
         "${DIR}/${1}/stop_cobalt.sh"         \
         "${DIR}/${1}/stop_process.sh")

for script in "${SCRIPTS[@]}"; do
  if [[ ! -f "${script}" ]]; then
    log "error" "The script '${script}' is required"
    exit 1
  fi

  source $script "${DIR}/${1}"
done
