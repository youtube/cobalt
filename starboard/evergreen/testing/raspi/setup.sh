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

source $1/../pprint.sh
source $1/run_command.sh

CONTENT="/home/pi/coeg/content/app/cobalt/content"
STORAGE_DIR="/home/pi/.cobalt_storage"
STORAGE_DIR_TMPFS="${STORAGE_DIR}.tmpfs"

ID="id"
TAIL="tail"

if [[ -z "${RASPI_ADDR}" ]]; then
  info "Please set the environment variable 'RASPI_ADDR'"
  exit 1
fi

KEYPATH="${HOME}/.ssh/raspi"

if [[ ! -f "${KEYPATH}" ]]; then
  ssh-keygen -t rsa -q -f "${KEYPATH}" -N "" 1> /dev/null
  ssh-copy-id -i "${KEYPATH}.pub" pi@"${RASPI_ADDR}" 1> /dev/null
  info "Generated SSH key-pair for Raspberry Pi 2 at ${KEYPATH}"
else
  info "Re-using existing SSH key-pair for Raspberry Pi 2 at ${KEYPATH}"
fi

SSH="ssh -i ${KEYPATH} pi@${RASPI_ADDR} "
SCP="scp -i ${KEYPATH} "

info "Targeting the Raspberry Pi 2 at ${RASPI_ADDR}"

# Attempt to check for a temporary filesystem at |STORAGE_DIR_TMPFS|, and try to
# create and mount it if not.
eval "${SSH}\"grep -qs \"${STORAGE_DIR_TMPFS}\" \"/proc/mounts\"\"" 1> /dev/null

if [[ $? -ne 0 ]]; then
  echo " Missing tmpfs mount at ${STORAGE_DIR_TMPFS}"

  if [[ "$(eval "${SSH}\"id -u\"")" -ne 0 ]]; then
    echo " Not root, cannot mount tmpfs at ${STORAGE_DIR_TMPFS}"
    return
  fi

  run_command "rm -rf \"${STORAGE_DIR_TMPFS}\""

  run_command "mkdir -p \"${STORAGE_DIR_TMPFS}\""

  run_command "sudo mount -F -t tmpfs -o size=10m \"cobalt_storage.tmpfs\" \"${STORAGE_DIR_TMPFS}\""
fi

