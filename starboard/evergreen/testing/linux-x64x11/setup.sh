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

CACHE_DIR="${HOME}/.cache/cobalt"
CONTENT="${OUT}/content/app/cobalt/content"
STORAGE_DIR="${HOME}/.cobalt_storage"
STORAGE_DIR_TMPFS="${STORAGE_DIR}.tmpfs"

ID="id"
TAIL="tail"
WC="wc"

if [[ -L "${STORAGE_DIR}" ]]; then
  rm -f ${STORAGE_DIR} 1> /dev/null
fi

# If it's possible to mount a temporary filesystem then the temporary filesystem
# is checked for and created + mounted if it does not exist. It's assumed to be
# possible unless explicitly declared otherwise via the CAN_MOUNT_TMPFS
# environment variable.
if [[ -z "${CAN_MOUNT_TMPFS}" ]] || [[ "${CAN_MOUNT_TMPFS}" -eq 1 ]]; then
  if ! grep -qs "${STORAGE_DIR_TMPFS}" "/proc/mounts"; then
    echo " Missing tmpfs mount at ${STORAGE_DIR_TMPFS}"

    if [[ ! -e "${STORAGE_DIR_TMPFS}" ]]; then
      mkdir -p "${STORAGE_DIR_TMPFS}" 1> /dev/null
    fi

    if [[ "$(${ID} -u)" -ne 0 ]]; then
      echo " Not root, trying to mount tmpfs with sudo at ${STORAGE_DIR_TMPFS}"
      sudo mount -F -t tmpfs -o size=10m "cobalt_storage.tmpfs" "${STORAGE_DIR_TMPFS}" 1> /dev/null
    else
      echo " Mounting tmpfs as root at ${STORAGE_DIR_TMPFS}"
      mount -F -t tmpfs -o size=10m "cobalt_storage.tmpfs" "${STORAGE_DIR_TMPFS}" 1> /dev/null
    fi
  fi
fi
