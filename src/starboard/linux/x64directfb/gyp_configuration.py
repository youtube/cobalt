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
"""Starboard Linux X64 DirectFB platform configuration."""

from starboard.linux.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter


class CobaltLinuxX64DirectFbConfiguration(
    shared_configuration.LinuxConfiguration):

  # Unfortunately, some memory leaks outside of our control, and difficult
  # to pattern match with ASAN's suppression list, appear in DirectFB
  # builds, and so ASAN must be disabled.
  def __init__(self,
               platform='linux-x64directfb',
               asan_enabled_by_default=False,
               goma_supported_by_compiler=True):
    super(CobaltLinuxX64DirectFbConfiguration,
          self).__init__(platform, asan_enabled_by_default,
                         goma_supported_by_compiler)

  def GetTestFilters(self):
    filters = (
        super(CobaltLinuxX64DirectFbConfiguration, self).GetTestFilters())
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # All filtered tests are filtered because the DirectFB drivers on
  # many Linux distributions are unstable and experience crashes when
  # creating and working with SbWindow objects.
  __FILTERED_TESTS = {
      'nplb': [
          'SbBlitterCreateSwapChainFromWindowTest.RainyDayBadDevice',
          'SbBlitterCreateSwapChainFromWindowTest.RainyDayBadWindow',
          'SbBlitterCreateSwapChainFromWindowTest.RainyDayInvalidSwapChain',
          'SbBlitterCreateSwapChainFromWindowTest.SunnyDay',
          'SbBlitterCreateSwapChainFromWindowTest.SunnyDayMultipleTimes',
          'SbBlitterFlipSwapChainTest.SunnyDay',
          'SbBlitterGetRenderTargetFromSwapChainTest.SunnyDay',
          'SbBlitterGetRenderTargetFromSwapChainTest.SunnyDayCanDraw',
          # FakeGraphicsContextProvider does not support DirectFB currently.
          'SbMediaSetAudioWriteDurationTests/SbMediaSetAudioWriteDurationTest.*',
          'SbPlayerTest.AudioOnly',
          'SbPlayerTest.Audioless',
          'SbPlayerTest.MultiPlayer',
          'SbPlayerTest.NullCallbacks',
          'SbPlayerTest.SunnyDay',
          'SbWindowCreateTest.SunnyDayDefault',
          'SbWindowCreateTest.SunnyDayDefaultSet',
          'SbWindowGetPlatformHandleTest.RainyDay',
          'SbWindowGetPlatformHandleTest.SunnyDay',
          'SbWindowGetSizeTest.RainyDayInvalid',
          'SbWindowGetSizeTest.SunnyDay',
      ],
      'player_filter_tests': [test_filter.FILTER_ALL],
  }


def CreatePlatformConfig():
  return CobaltLinuxX64DirectFbConfiguration()
