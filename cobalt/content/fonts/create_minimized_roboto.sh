#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
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

# This script can be used as a convenience to launch an app under a Xephyr
# X server.  It will first launch Xephyr, and then launch the executable given
# on the command line such that it targets and will appear within the Xephyr
# window.  It shuts down Xephyr after the main executable finishes.
set -e

SCRIPT_FILE="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "${script_file}")"
SCRIPT_NAME="$(basename "${script_file}")"

GLPHYIGO_SRC=https://raw.githubusercontent.com/pettarin/glyphIgo/bb91c5f3746bcb17395acb6ffd436c982c41bd8a/src/glyphIgo.py

function log() {
  echo "${SCRIPT_NAME}: $@"
}

function deleteDirectory() {
  if [[ -z "$1" ]]; then
    log "deleteDirectory with no argument"
    exit 1
  fi

  # Only delete target if it is an existing directory.
  if [[ -d "$1" ]]; then
    log "Deleting directory: $1"
    rm -rf "$1"
  fi
}

function deleteTempTrap() {
  # If something interrupted the script, it might have printed a partial line.
  echo
  deleteDirectory "${TEMP_DIR}"
}

TEMP_DIR="$(mktemp -dt "${SCRIPT_NAME}.XXXXXXXXXX")"
trap deleteTempTrap EXIT

TEMP_GLYPHIGO="$TEMP_DIR/glyphIgo.py"

# download the file
curl -sSL $GLPHYIGO_SRC -o $TEMP_GLYPHIGO

python $TEMP_GLYPHIGO subset --plain \
  "$SCRIPT_DIR/minimized_roboto_subset_chars.txt" \
  -f "$SCRIPT_DIR/Roboto-Regular.ttf" \
  -o "$SCRIPT_DIR/minimized_roboto_needs_tofu.ttf"

# Clear delete trap now that we have succeeded.
trap - EXIT

# Delete our temp directory on graceful exit.
deleteDirectory "${TEMP_DIR}"

