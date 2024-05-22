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
"""Starboard Linux X64 X11 platform configuration."""

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter


class LinuxX64X11Configuration(shared_configuration.LinuxConfiguration):
  """Starboard Linux X64 X11 platform configuration."""

  def GetTestTargets(self):
    return test_filter.EVERGREEN_COMPATIBLE_TESTS + super().GetTestTargets()


def CreatePlatformConfig():
  return LinuxX64X11Configuration('linux-x64x11')
