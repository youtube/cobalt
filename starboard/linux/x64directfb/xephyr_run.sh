#!/usr/bin/env bash
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

# This script can be used as a convenience to launch an app under a new
# X server.  Xephyr requires a base X server, so will be launched if there is a
# DISPLAY environment variable set. Xvfb does not, so it will be launched if
# there is no DISPLAY variable set, which is likely to come from a remote shell
# or background process. It will first launch the X server, and then launch the
# executable given on the command line such that it targets the newly-launched
# X server. It shuts down the X server after the main executable finishes.

# Standard stuff to get the real script name.
script_file="$(readlink -f "${BASH_SOURCE[0]}")"
script_name="$(basename "${script_file}")"

if [[ -n "$DISPLAY" ]]; then
  # The binary to run to start the X Server.
  xserver_bin=Xephyr
  # The command-line args to set the screen dimensions.
  declare -a xserver_screen=(-screen 1920x1080)
  # The package to install for this X Server.
  xserver_package=xserver-xephyr
else
  xserver_bin=Xvfb
  declare -a xserver_screen=(-screen 0 1920x1080x24)
  xserver_package=xvfb
fi

# Use display 42 as it is unlikely to be used by another process on this host.
# TODO: Scan displays for a free one.
xserver_display=":42"

function log() {
  echo "${script_name}: $@"
}

function deleteTempTrap() {
  # If something interrupted the script, it might have printed a partial line.
  echo
  deleteDirectory "${temp_dir}"
}

function killServerTrap() {
  echo
  # Kill the Xephyr process.
  kill "${xserver_pid}" &> /dev/null
  # Waits for the kill to finish.
  wait
  log "${xserver_bin} (pid ${xserver_pid}) terminated."
  deleteDirectory "${temp_dir}"
}

function deleteDirectory() {
  if [[ -z "$1" ]]; then
    log "deleteDirectory with no argument"
    exit 1
  fi

  # Only delete target if it is an existing directory.
  if [[ -d "$1" ]]; then
    rm -rf "$1"
  fi
}

function main() {
  if [[ "$#" = "0" ]]; then
    echo
    echo "${script_name}: Launches a given executable file under a ${xserver_bin} X server."
    echo
    echo "Usage:"
    echo "  ${script_name} PATH_TO_EXECUTABLE_FILE [ARGS...] "
    echo
    exit 1
  fi

  if ! hash "${xserver_bin}" 2>/dev/null ; then
    log "${xserver_bin} is not installed.  Please run:"
    log "  sudo apt-get install ${xserver_package}"
    exit 1
  fi

  # Create an auth file that will allow the current user to access the display.
  temp_dir="$(mktemp -dt "${script_name}.XXXXXXXXXX")"

  # Delete the temporary directory on exit.
  trap deleteTempTrap EXIT

  # In this case, we don't want to try to tunnel authority through to remote
  # connections, we want to use this X server with the client. So we create
  # our own auth and forcibly pass it through both sides of the client and
  # server. Apologies for the X11 magic here.
  xserver_auth="${temp_dir}/XAuthority"
  touch "${xserver_auth}"
  token="$(dd if=/dev/urandom count=1 2> /dev/null | md5sum | cut -f1 -d' ')"
  xauth -qf "${xserver_auth}" add "${xserver_display}" . "${token}"

  xserver_log="${temp_dir}/${xserver_bin}.txt"
  # Launch an X Server in the background at a new display.
  "${xserver_bin}" "${xserver_display}" \
    -auth "${xserver_auth}" \
    "${xserver_screen[@]}" \
    &> "${xserver_log}" &
  xserver_pid=$!
  log "${xserver_bin} (pid ${xserver_pid}) running on ${xserver_display}."

  # Setup a trap to clean up Xephyr upon termination of this script
  trap killServerTrap EXIT

  # Give Xephyr some time to setup.
  sleep 0.5

  # Launch the executable passed as a parameter on the new display.
  DISPLAY="${xserver_display}" XAUTHORITY="${xserver_auth}" "$@"
  result=$?
  if [[ "${result}" != "0" ]]; then
    log "$1 result: ${result}"
    log "--- ${xserver_bin} log ---------------------------------------"
    cat "${xserver_log}"
  fi
  return ${result}
}

main "$@"
