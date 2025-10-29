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

_APP_IDENTIFIER_PREFIX = "foo.cobalt."
# Simulator type is used to locate the simulator's device ID of the specified
# platform. Change this to switch simulator type. Make this an input if we
# want to run tests on multiple simulators.
_SIMULATOR_TYPE = "Apple TV 4K"
_DEVICE_ID_RE = re.compile(_SIMULATOR_TYPE +
                           r".* \(([-0-9A-F]*)\) \((Booted|Shutdown)\)\s*$")


class DeviceIDNotFoundError(Exception):

  def __init__(self):
    super().__init__("Simulator Device ID not found on machine.")


class SimulatorPathNotFoundError(Exception):

  def __init__(self):
    super().__init__("Simulator Path not found, did you install xcode?")


def _GetDeviceIdMatch():
  id_list = subprocess.check_output(["xcrun", "simctl", "list"]).decode("utf-8")
  for line in id_list.split("\n"):
    device_id = _DEVICE_ID_RE.search(line)
    if device_id:
      return device_id
  raise DeviceIDNotFoundError()


def GetSimulatorID():
  return _GetDeviceIdMatch().group(1)


def IsSimulatorRunning():
  return _GetDeviceIdMatch().group(2) == "Booted"


class Launcher(TvOSLauncher):
  """Run an application on tvOS simulator."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    self.device_id = None

    super().__init__(platform, target_name, config, device_id, **kwargs)

    if not self.device_id:
      self.device_id = GetSimulatorID()

  def _HasTools(self):
    xcrun_version = subprocess.check_output(["xcrun", "--version"]).decode()
    xcode_version = subprocess.check_output(["xcode-select",
                                             "--version"]).decode()
    # If xcode command line tools are installed, this command should return
    # version number.
    if xcrun_version.find("command not found") >= 0 or xcode_version.find(
        "command not found") >= 0:
      print("Please install xcode command line tools and make sure "
            "xcode-select and xcrun tools both work. Exiting app launcher...")
      return 0
    return 1

  def _CleanUp(self):
    # Uninstall the test app.
    subprocess.call([
        "xcrun", "simctl", "uninstall", self.device_id,
        _APP_IDENTIFIER_PREFIX + self.target_name
    ])
    if self.test_output_stream:
      self.test_output_stream.close()

  def _FilterOutput(self, line):
    return line

  def _LaunchTest(self):
    # Ensure the simulator is running.
    if not IsSimulatorRunning():
      print(f"Starting simulator: {_SIMULATOR_TYPE} {self.device_id}.")
      return_code = subprocess.call(["xcrun", "simctl", "boot", self.device_id])
      if return_code != 0:
        return False
    else:
      print(f"Simulator already started: {_SIMULATOR_TYPE} {self.device_id}.")
    # Install the app onto the simulator.
    return_code = subprocess.call([
        "xcrun", "simctl", "install", self.device_id,
        self.GetInstallTargetPath() + ".app"
    ])
    if return_code != 0:
      return False
    # Launch the app.
    self.test_process = subprocess.Popen(  # pylint: disable=consider-using-with
        [
            "xcrun", "simctl", "launch", "--console-pty", self.device_id,
            _APP_IDENTIFIER_PREFIX + self.target_name
        ] + self.target_command_line_params,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    self.test_output_stream = self.test_process.stdout
    return True
