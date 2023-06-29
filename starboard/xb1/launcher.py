#
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""XB1 implementation of Starboard launcher abstraction."""

from __future__ import print_function

from starboard.tools import abstract_launcher
from starboard.xb1.tools import uwp_launcher
from starboard.xb1.tools import xb1_launcher

_UWP_XB1_MESSAGE = '****************************************************\n' + \
                   '*                    UWP on XB1                    *\n' + \
                   '****************************************************\n'

_UWP_WIN_MESSAGE = '****************************************************\n' + \
                   '*                  UWP on Windows                  *\n' + \
                   '****************************************************\n'


# This Launcher delegates either to an xb1 launcher or to a uwp launcher.
# The xb1 launcher runs tests on the xb1 remote device.
# The uwp launcher runs tests on the local windows device.
class Launcher(abstract_launcher.AbstractLauncher):
  """Run an application on XB1."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    super().__init__(platform, target_name, config, device_id, **kwargs)
    if not device_id:
      raise ValueError('\nMissing device_id, please specify --device_id '
                       '<XboxIP> or --device_id win_uwp (running in Windows).')
    if device_id == 'win_uwp':
      self.output_file.write('\n' + _UWP_WIN_MESSAGE)
      self.delegate = uwp_launcher.Launcher(platform, target_name, config,
                                            device_id, **kwargs)
    else:
      # Assume the address points to an xbox.
      self.output_file.write('\n' + _UWP_XB1_MESSAGE)
      # Defer loading because depot_tools does not have the required
      # python requests library installed.
      # TODO: Install requests library depot_tools and update.
      self.delegate = xb1_launcher.Launcher(platform, target_name, config,
                                            device_id, **kwargs)

  # Run() and Kill() needs to be explicitly implemented per AbstractLauncher.
  def Run(self):
    return self.delegate.Run()

  def Kill(self):
    return self.delegate.Kill()

  # All other functions are automatically delegated using this function.
  def __getattr__(self, fname):

    def method(*args):
      f = getattr(self.delegate, fname)
      return f(*args)

    return method

  def GetDeviceIp(self):
    """Gets the device IP. TODO: Implement."""
    return None

  def GetDeviceOutputPath(self):
    self.delegate.GetDeviceOutputPath()
