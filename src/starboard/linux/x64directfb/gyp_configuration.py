# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Starboard Linux X64 DirectFB platform configuration."""

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter


class CobaltLinuxX64DirectFbConfiguration(
    shared_configuration.LinuxConfiguration):

  # Unfortunately, some memory leaks outside of our control, and difficult
  # to pattern match with ASAN's suppression list, appear in DirectFB
  # builds, and so ASAN must be disabled.
  def __init__(self, platform='linux-x64directfb',
               asan_enabled_by_default=False,
               goma_supported_by_compiler=True):
    super(CobaltLinuxX64DirectFbConfiguration, self).__init__(
        platform, asan_enabled_by_default, goma_supported_by_compiler)

  def GetTestFilters(self):
    filters = super(CobaltLinuxX64DirectFbConfiguration, self).GetTestFilters()
    filters.extend([
        test_filter.TestFilter(
            'starboard_platform_tests', test_filter.FILTER_ALL),
    ])
    return filters


def CreatePlatformConfig():
  return CobaltLinuxX64DirectFbConfiguration()
