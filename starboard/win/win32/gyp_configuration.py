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
"""Starboard win-win32 platform configuration for gyp_cobalt."""

from __future__ import print_function

import imp
import logging
import os
import sys

from starboard.tools.testing import test_filter

# Import the shared win platform configuration.
sys.path.append(
    os.path.realpath(
        os.path.join(
            os.path.dirname(__file__), os.pardir, os.pardir, 'shared',
            'win32')))
import gyp_configuration


def CreatePlatformConfig():
  try:
    win_lib_config = WinWin32PlatformConfig('win-win32')
    return win_lib_config
  except RuntimeError as e:
    logging.critical(e)
    return None


class WinWin32PlatformConfig(gyp_configuration.Win32SharedConfiguration):
  """Starboard win-32 platform configuration."""

  def __init__(self, platform):
    super(WinWin32PlatformConfig, self).__init__(platform)

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

    if not self.IsWin10orHigher():
      logging.error('Tests can only be executed on Win10 and higher.')
      return [test_filter.DISABLE_TESTING]
    else:
      filters = super(WinWin32PlatformConfig, self).GetTestFilters()
      for target, tests in self._FILTERED_TESTS.iteritems():
        filters.extend(test_filter.TestFilter(target, test) for test in tests)
      return filters

  _FILTERED_TESTS = {
      'renderer_test': [
          'ResourceProviderTest.ManyTexturesCanBeCreatedAndDestroyedQuickly', # Flaky.
          'ResourceProviderTest.TexturesCanBeCreatedFromSecondaryThread',
          'PixelTest.Width1Image',
          'PixelTest.Height1Image',
          'PixelTest.Area1Image',
      ],

      'nplb': [
          # TODO: Check these unit tests and fix them!
          'SbAudioSinkCreateTest.MultiSink',
          'SbAudioSinkCreateTest.RainyDayInvalid*',
          'SbAudioSinkCreateTest.SunnyDay',
          'SbAudioSinkCreateTest.SunnyDayAllCombinations',
          'SbAudioSinkIsAudioSampleTypeSupportedTest.SunnyDay',
          'SbAudioSinkTest.*',

          'SbMicrophoneCloseTest.SunnyDayCloseAreCalledMultipleTimes',
          'SbMicrophoneOpenTest.SunnyDay',
          'SbMicrophoneOpenTest.SunnyDayNoClose',
          'SbMicrophoneOpenTest.SunnyDayMultipleOpenCalls',
          'SbMicrophoneReadTest.SunnyDay',
          'SbMicrophoneReadTest.SunnyDayReadIsLargerThanMinReadSize',
          'SbMicrophoneReadTest.RainyDayAudioBufferIsNULL',
          'SbMicrophoneReadTest.RainyDayAudioBufferSizeIsSmallerThanMinReadSize',
          'SbMicrophoneReadTest.RainyDayAudioBufferSizeIsSmallerThanRequestedSize',
          'SbMicrophoneReadTest.RainyDayOpenCloseAndRead',
          'SbMicrophoneReadTest.SunnyDayOpenSleepCloseAndOpenRead',

          'SbPlayerTest.Audioless',
          'SbPlayerTest.AudioOnly',
          'SbPlayerTest.NullCallbacks',
          'SbPlayerTest.MultiPlayer',
          'SbPlayerTest.SunnyDay',

          'SbConditionVariableWaitTimedTest.SunnyDayAutoInit', # Flaky

          'SbSocketJoinMulticastGroupTest.SunnyDay',
          'SbSystemGetStackTest.SunnyDayStackDirection',
          # Fails when subtracting infinity and expecting NaN:
          # for EXPECT_TRUE(SbDoubleIsNan(infinity + -infinity))
          'SbUnsafeMathTest.NaNDoubleSunnyDay',
       ],

       'net_unittests': [
           'FileStreamTest.AsyncRead_EarlyDelete',
           'FileStreamTest.AsyncOpenAndDelete',
           'FileStreamTest.AsyncWrite_EarlyDelete',
           'HostResolverImplDnsTest.DnsTaskUnspec',
           'UDPListenSocketTest.DoRead',
           'UDPListenSocketTest.DoReadReturnsNullAtEnd',
           'UDPListenSocketTest.SendTo',
       ],

       'web_platform_tests': [
          '*WebPlatformTest.Run*',
       ],

      'player_filter_tests': [ test_filter.FILTER_ALL ],
  }

  def GetVariables(self, configuration):
    variables = super(WinWin32PlatformConfig, self).GetVariables(configuration)
    # These variables will tell gyp to compile with V8.
    variables.update({
        'javascript_engine': 'v8',
        'cobalt_enable_jit': 1,
    })
    return variables
