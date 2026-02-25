#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
"""tvOS simulator implementation of Starboard launcher abstraction."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import re
import subprocess

from internal.starboard.darwin.tvos.shared.tvos_launcher import TvOSLauncher

# We will use the same bundle name for every unit test for convenience.
_BUNDLE_NAME = "cobalt_unit_test"
_APP_IDENTIFIER_PREFIX = "foo.cobalt."

# Regex to filter ios-deploy's "copy to device" spam.
_RE_DEPLOY_SPAM = re.compile(r"^\[.+%\] Copying .+ to device$")

# Timeout in seconds to detect connected devices.
_DETECT_TIMEOUT = "2"

# Regex to parse the device IDs from the ios-deploy --detect output.
_RE_DETECT_ID = re.compile(r"Found ([A-Fa-f0-9]+) ")


class Launcher(TvOSLauncher):
  """Run an application on tvOS arm64 device."""

  def _HasTools(self):
    ios_deploy_str = subprocess.check_output(["ios-deploy",
                                              "--version"]).decode()
    xcode_str = subprocess.check_output(["xcode-select", "--version"]).decode()
    # If xcode command line tools are installed, this command should return
    # version number.
    if ios_deploy_str.find("command not found") >= 0 or xcode_str.find(
        "command"
        " not found") >= 0:
      print("Needed tools missing. Please follow all steps in"
            "starboard/darwin/tvos/shared/PRE_TEST_SETUP.md."
            "Exiting app launcher...")
      return 0
    return 1

  def _FilterOutput(self, line):
    # Remove ios-deploy's "copying to device" spam.
    if _RE_DEPLOY_SPAM.match(line):
      return None
    else:
      return line

  def _IsDeviceConnected(self):
    """Check if a device is connected."""
    command_line = ["ios-deploy", "--detect", "--timeout", _DETECT_TIMEOUT]
    detect_output = subprocess.check_output(command_line).decode().split("\n")
    ids = []
    for line in detect_output:
      match = _RE_DETECT_ID.search(line)
      if match:
        ids.append(match.group(1).lower())
    if self.device_id:
      return self.device_id.lower() in ids
    return bool(ids)

  def _CleanUp(self):
    if not self._IsDeviceConnected():
      print("Device is not connected. Aborting cleanup.")
      return
    command_line = [
        "ios-deploy", "--uninstall_only", "--bundle_id",
        _APP_IDENTIFIER_PREFIX + self.target_name
    ]
    if self.device_id:
      command_line += ["--id", self.device_id]
    subprocess.call(command_line)

  def _LaunchTest(self):
    if not self._IsDeviceConnected():
      if self.device_id:
        print(f"ERROR: Device {self.device_id} is not connected!")
      else:
        print("ERROR: No connected device found!")
      return False
    command_line = [
        "ios-deploy", "--noninteractive", "--debug", "--bundle",
        self.GetInstallTargetPath() + ".app"
    ]
    if self.device_id:
      command_line += ["--id", self.device_id]
    if self.target_command_line_params:
      command_line += ["--args", " ".join(self.target_command_line_params)]
    self.test_process = subprocess.Popen(  # pylint: disable=consider-using-with
        command_line,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    self.test_output_stream = self.test_process.stdout
    return True
