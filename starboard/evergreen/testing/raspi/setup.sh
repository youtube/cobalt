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

CACHE_DIR="${HOME}/.cache/cobalt"
CONTENT="/home/pi/coeg/content/app/cobalt/content"
STORAGE_DIR="/home/pi/.cobalt_storage"
STORAGE_DIR_TMPFS="${STORAGE_DIR}.tmpfs"

ID="id"
TAIL="tail"
WC="wc"

if [[ -z "${RASPI_ADDR}" ]]; then
  log "info" " Please set the environment variable 'RASPI_ADDR'"
  exit 1
fi

KEYPATH="${HOME}/.ssh/raspi"

if [[ ! -f "${KEYPATH}" ]]; then
  ssh-keygen -t rsa -q -f "${KEYPATH}" -N "" 1> /dev/null
  ssh-copy-id -i "${KEYPATH}.pub" pi@"${RASPI_ADDR}" 1> /dev/null
  echo " Generated SSH key-pair for Raspberry Pi 2 at ${KEYPATH}"
else
  echo " Re-using existing SSH key-pair for Raspberry Pi 2 at ${KEYPATH}"
fi

SSH="ssh -i ${KEYPATH} pi@${RASPI_ADDR} "
SCP="scp -i ${KEYPATH} "

echo " Targeting the Raspberry Pi 2 at ${RASPI_ADDR}"

# Attempt to unlink the path, ignoring errors.
eval "${SSH}\"unlink \"${STORAGE_DIR}\"\"" 1> /dev/null

# Mounting a temporary filesystem cannot be done on buildbot since it requires
# sudo. When run locally, check for the temporary filesystem and create and
# mount it if it does not exist.
if [[ -z "${IS_BUILDBOT}" ]] || [[ "${IS_BUILDBOT}" -eq 0 ]]; then
  eval "${SSH}\"grep -qs \"${STORAGE_DIR_TMPFS}\" \"/proc/mounts\"\"" 1> /dev/null

  if [[ $? -ne 0 ]]; then
    if [[ ! -e "${STORAGE_DIR_TMPFS}" ]]; then
      run_command "mkdir -p \"${STORAGE_DIR_TMPFS}\"" 1> /dev/null
    fi

    if [[ "$(${SSH} "${ID} -u")" -ne 0 ]]; then
      echo " Not root, trying to mount tmpfs with sudo at ${STORAGE_DIR_TMPFS}"
      run_command "sudo mount -F -t tmpfs -o size=10m \"cobalt_storage.tmpfs\" \"${STORAGE_DIR_TMPFS}\"" 1> /dev/null
    else
      echo " Mounting tmpfs as root at ${STORAGE_DIR_TMPFS}"
      run_command "mount -F -t tmpfs -o size=10m \"cobalt_storage.tmpfs\" \"${STORAGE_DIR_TMPFS}\"" 1> /dev/null
    fi
  fi
fi
