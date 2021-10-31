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

CACHE_DIR="/home/pi/.cache/cobalt"
CONTENT="/home/pi/coeg/content/app/cobalt/content"
STORAGE_DIR="/home/pi/.cobalt_storage"
STORAGE_DIR_TMPFS="${STORAGE_DIR}.tmpfs"
PID_STORAGE_DIR="/tmp/cobalt_loader_pids"

ID="id"
TAIL="tail"
WC="wc"

# Prioritize the command-line argument over the environment variable.
if [[ ! -z "${DEVICE_ID}" ]]; then
  RASPI_ADDR="${DEVICE_ID}"
fi

if [[ -z "${RASPI_ADDR}" ]]; then
  log "info" " Please pass in the device id or set the environment variable 'RASPI_ADDR'"
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

# If it's possible to mount a temporary filesystem then the temporary filesystem
# is checked for and created + mounted if it does not exist. It's assumed to be
# possible unless explicitly declared otherwise via the CAN_MOUNT_TMPFS
# environment variable.
if [[ -z "${CAN_MOUNT_TMPFS}" ]] || [[ "${CAN_MOUNT_TMPFS}" -eq 1 ]]; then
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

eval "${SSH}\"test -d /tmp\"" 1> /dev/null
if [[ $? -ne 0 ]]; then
  echo " The '/tmp' directory is required on the Raspberry Pi 2 to persist data"
  exit 1
fi

echo " Making a directory on the Raspberry Pi 2 to store loader app process ids"
run_command "mkdir -p ${PID_STORAGE_DIR}"
