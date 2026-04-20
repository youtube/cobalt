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

  staging_dir="${OUT}/install/loader_app"

  echo " Checking '${staging_dir}'"

  PATHS=("${staging_dir}/loader_app"                                                \
         "${staging_dir}/content/app/cobalt/lib/libcobalt${SYSTEM_IMAGE_EXTENSION}" \
         "${staging_dir}/content/app/cobalt/content/")

  for file in "${PATHS[@]}"; do
    if [[ ! -e "${file}" ]]; then
      echo " Failed to find '${file}'"
      exit 1
    fi
  done

  echo " Required files were found within '${OUT}'"

  echo " Deploying to the Raspberry Pi 2 at ${RASPI_ADDR}"

  echo " Regenerating Cobalt-on-Evergreen directory"
  eval "${SSH} \"rm -rf /home/pi/coeg/\""
  eval "${SSH} \"mkdir /home/pi/coeg/\""

  echo " Copying loader_app to Cobalt-on-Evergreen directory"
  eval "${SCP} \"${staging_dir}/loader_app pi@${RASPI_ADDR}:/home/pi/coeg/\""

  echo " Copying crashpad_handler to Cobalt-on-Evergreen directory"
  eval "${SCP} \"${staging_dir}/crashpad_handler pi@${RASPI_ADDR}:/home/pi/coeg/\""

  echo " Regenerating system image directory"
  eval "${SSH} \"mkdir -p /home/pi/coeg/content/app/cobalt/lib\""

  echo " Copying cobalt to system image directory"
  eval "${SCP} \"${staging_dir}/content/app/cobalt/lib/libcobalt${SYSTEM_IMAGE_EXTENSION} pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/lib/\""

  echo " Copying content to system image directory"
  eval "${SCP} \"-r ${staging_dir}/content/app/cobalt/content/ pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/\""

  echo " Copying fonts to system content directory"
  eval "${SCP} \"-r ${staging_dir}/content/fonts/ pi@${RASPI_ADDR}:/home/pi/coeg/content/\""

  echo " Generating HTML test directory"
  eval "${SSH} \"mkdir -p /home/pi/coeg/content/app/cobalt/content/web/tests/\""

  echo " Copying HTML test files to HTML test directory"
  eval "${SCP} \"${1}/../tests/empty.html pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/content/web/tests/\""
  eval "${SCP} \"${1}/../tests/test.html pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/content/web/tests/\""
  eval "${SCP} \"${1}/../tests/tseries.html pi@${RASPI_ADDR}:/home/pi/coeg/content/app/cobalt/content/web/tests/\""

  echo " Successfully deployed!"
}
