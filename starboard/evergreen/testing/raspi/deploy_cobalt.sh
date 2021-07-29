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

function deploy_cobalt() {
  if [[ -z "${OUT}" ]]; then
    log "info" "Please set the environment variable 'OUT'"
    exit 1
  fi

  echo " Checking '${OUT}'"

  PATHS=("${OUT}/deploy/loader_app/loader_app"                          \
         "${OUT}/deploy/loader_app/content/app/cobalt/lib/libcobalt.so" \
         "${OUT}/deploy/loader_app/content/app/cobalt/content/")

  for file in "${PATHS[@]}"; do
    if [[ ! -e "${file}" ]]; then
      echo " Failed to find '${file}'"
      exit 1
    fi
  done

  echo " Required files were found within '${OUT}'"

  echo " Deploying to the Raspberry Pi 2 at ${RASPI_ADDR}"

  echo " Regenerating Cobalt-on-Evergreen directory"
  eval "${SSH} rm -rf /home/pi/coeg/" 1> /dev/null
  eval "${SSH} mkdir /home/pi/coeg/" 1> /dev/null

  echo " Copying loader_app to Cobalt-on-Evergreen directory"
  eval "${SCP} ${OUT}/deploy/loader_app/loader_app pi@${RASPI_ADDR}:/home/pi/coeg/" 1> /dev/null

  echo " Copying crashpad_handler to Cobalt-on-Evergreen directory"
  eval "${SCP} ${OUT}/deploy/loader_app/crashpad_handler pi@${RASPI_ADDR}:/home/pi/coeg/" 1> /dev/null

  echo " Regenerating system image directory"
  eval "${SSH} mkdir -p /home/pi/coeg/content/app/cobalt/lib" 1> /dev/null

  echo " Copying cobalt to system image directory"
  eval "${SCP} ${OUT}/deploy/loader_app/content/app/cobalt/lib/libcobalt.so pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/lib/" 1> /dev/null

  echo " Copying content to system image directory"
  eval "${SCP} -r ${OUT}/deploy/loader_app/content/app/cobalt/content/ pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/" 1> /dev/null

  echo " Copying fonts to system content directory"
  eval "${SCP} -r ${OUT}/content/fonts/ pi@${RASPI_ADDR}:/home/pi/coeg/content/" 1> /dev/null

  echo " Generating HTML test directory"
  eval "${SSH} mkdir -p /home/pi/coeg/content/app/cobalt/content/web/tests/" 1> /dev/null

  echo " Copying HTML test files to HTML test directory"
  eval "${SCP} ${1}/../tests/*.html pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/content/web/tests/" 1> /dev/null

  echo " Successfully deployed!"
}
