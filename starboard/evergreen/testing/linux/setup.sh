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

CONTENT="${OUT}/content/app/cobalt/content"
STORAGE_DIR="${HOME}/.cobalt_storage"
STORAGE_DIR_TMPFS="${STORAGE_DIR}.tmpfs"

ID="id"
TAIL="tail"

# Attempt to check for a temporary filesystem at |STORAGE_DIR_TMPFS|, and try to
# create and mount it if not.
if ! grep -qs "${STORAGE_DIR_TMPFS}" "/proc/mounts"; then
  echo " Missing tmpfs mount at ${STORAGE_DIR_TMPFS}"

  if [[ "$(id -u)" -ne 0 ]]; then
    echo " Not root, cannot mount tmpfs at ${STORAGE_DIR_TMPFS}"
    return
  fi

  rm -rf "${STORAGE_DIR_TMPFS}"

  mkdir -p "${STORAGE_DIR_TMPFS}"

  sudo mount -F -t tmpfs -o size=10m "cobalt_storage.tmpfs" "${STORAGE_DIR_TMPFS}"
fi

