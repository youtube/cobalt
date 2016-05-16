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

# This script can be used as a convenience to launch an app under a Xephyr
# X server.  It will first launch Xephyr, and then launch the executable given
# on the command line such that it targets and will appear within the Xephyr
# window.  It shuts down Xephyr after the main executable finishes.

if [[ $# = 0 ]]; then
  echo
  echo "$0 will launch a given executable file under a Xephyr X server."
  echo
  echo "Usage:"
  echo "  $0 PATH_TO_EXECUTABLE_FILE  "
  echo
  exit 0
fi

if ! hash Xephyr 2>/dev/null ; then
  echo
  echo "Xephyr is not installed.  Please run:"
  echo "  sudo apt-get install xserver-xephyr"
  echo
  exit 0
fi

# Open up a Xephyr display.
Xephyr -screen 1920x1080 :10 2> /dev/null &
xephyr_pid=$!

# Setup a trap to clean up Xephyr upon termination of this script
function kill1() {
  # Kill the Xephyr process.
  kill $xephyr_pid 2> /dev/null
  # Waits for the kill to finish.
  wait
}
trap kill1 EXIT

# Give Xephyr some time to setup.
sleep 0.5

# Launch the executable passed as a parameter on the Xephyr display.
DISPLAY=:10 $@