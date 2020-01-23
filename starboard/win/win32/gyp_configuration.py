# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Starboard win-win32 platform configuration for gyp_cobalt."""

from __future__ import print_function

import imp
import logging
import os
import subprocess
import sys

import _env  # pylint: disable=unused-import

from starboard.tools.testing import test_filter
import starboard.shared.win32.gyp_configuration as gyp_configuration

def CreatePlatformConfig():
  try:
    win_lib_config = WinWin32PlatformConfig('win-win32', sabi_json_path='starboard/sabi/x64/windows/sabi.json')
    return win_lib_config
  except RuntimeError as e:
    logging.critical(e)
    return None


class WinWin32PlatformConfig(gyp_configuration.Win32SharedConfiguration):
  """Starboard win-32 platform configuration."""

  def __init__(self, platform, sabi_json_path=None):
    super(WinWin32PlatformConfig, self).__init__(platform, sabi_json_path=sabi_json_path)

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'launcher.py'))

    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    if os.environ.get('COBALT_WIN_BUILDBOT_DISABLE_TESTS', '0') == '1':
      logging.error('COBALT_WIN_BUILDBOT_DISABLE_TESTS=1, Tests are disabled.')
      return [test_filter.DISABLE_TESTING]
    else:
      filters = super(WinWin32PlatformConfig, self).GetTestFilters()
      for target, tests in self.__FILTERED_TESTS.iteritems():
        filters.extend(test_filter.TestFilter(target, test) for test in tests)
      return filters

  __FILTERED_TESTS = {
      'nplb': [
          # This single test takes >15 minutes.
          'SbPlayerTest.MultiPlayer',
          # This test fails on win-win32 devel builds, because the compiler
          # performs an optimization that defeats the SB_C_NOINLINE 'noinline'
          # attribute.
          'SbSystemGetStackTest.SunnyDayStackDirection',
       ],
      'player_filter_tests': [
          # This test fails on win-win32.
          'AudioDecoderTests/AudioDecoderTest.MultipleInvalidInput/0',
          # These tests fail on our VMs for win-win32 builds due to missing
          # or non functioning system video decoders.
          'VideoDecoderTests/VideoDecoderTest.PrerollFrameCount/0',
          'VideoDecoderTests/VideoDecoderTest.MaxNumberOfCachedFrames/0',
          'VideoDecoderTests/VideoDecoderTest.PrerollTimeout/0',
          'VideoDecoderTests/VideoDecoderTest.OutputModeSupported/0',
          'VideoDecoderTests/VideoDecoderTest.GetCurrentDecodeTargetBeforeWriteInputBuffer/0',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/0',
          'VideoDecoderTests/VideoDecoderTest.ThreeMoreDecoders/1',
          'VideoDecoderTests/VideoDecoderTest.SingleInput/0',
          'VideoDecoderTests/VideoDecoderTest.SingleInvalidKeyFrame/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleValidInputsAfterInvalidKeyFrame/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleInvalidInput/0',
          'VideoDecoderTests/VideoDecoderTest.EndOfStreamWithoutAnyInput/0',
          'VideoDecoderTests/VideoDecoderTest.ResetBeforeInput/0',
          'VideoDecoderTests/VideoDecoderTest.ResetAfterInput/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleResets/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleInputs/0',
          'VideoDecoderTests/VideoDecoderTest.Preroll/0',
          'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/0',
          'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/0',
],
  }

  def GetVariables(self, configuration):
    variables = super(WinWin32PlatformConfig, self).GetVariables(configuration)
    # These variables will tell gyp to compile with V8.
    variables.update({
        'javascript_engine': 'v8',
        'cobalt_enable_jit': 1,
    })
    return variables
