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
"""Starboard Creator CI20 X11 gcc 4.9 platform configuration for gyp_cobalt."""

import logging
import os
import subprocess

# pylint: disable=import-self,g-import-not-at-top
import gyp_utils
# Import the shared Linux platform configuration.
from starboard.creator.shared import gyp_configuration
from starboard.tools.testing import test_filter

class PlatformConfig(gyp_configuration.PlatformConfig):
  """Starboard Creator platform configuration."""

  def __init__(self, platform):
    super(PlatformConfig, self).__init__(platform)

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(configuration)
    variables.update({'clang': 0,})
    toolchain_lib_path = os.path.join(self.toolchain_dir, 'lib')
    variables.update({'toolchain_lib_path': toolchain_lib_path,})
    return variables

  def GetEnvironmentVariables(self):
    env_variables = super(PlatformConfig, self).GetEnvironmentVariables()
    self.toolchain_dir = os.path.join(self.ci20_home)
    toolchain_bin_dir = os.path.join(self.toolchain_dir, 'bin')
    env_variables.update({
        'CC': os.path.join(toolchain_bin_dir, 'mipsel-linux-gnu-gcc'),
        'CXX': os.path.join(toolchain_bin_dir, 'mipsel-linux-gnu-g++'),
        'CC_host': 'gcc',
        'CXX_host': 'g++',
        'LD_host': 'g++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    })
    return env_variables

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    return [
        # test is disabled on x64
        test_filter.TestFilter(
            'bindings_test', ('GlobalInterfaceBindingsTest.'
                              'PropertiesAndOperationsAreOwnProperties')),
        # tests miss the defined delay
        test_filter.TestFilter(
            'nplb', 'SbConditionVariableWaitTimedTest.SunnyDay'),
        test_filter.TestFilter(
            'nplb', 'SbConditionVariableWaitTimedTest.SunnyDayAutoInit'),
        # tests sometimes miss the threshold of 10ms
        test_filter.TestFilter(
            'nplb', 'Semaphore.ThreadTakesWait_PutBeforeTimeExpires'),
        test_filter.TestFilter(
            'nplb', 'RWLock.HoldsLockForTime'),
        # tests sometimes miss the threshold of 5ms
        test_filter.TestFilter(
            'nplb', 'Semaphore.ThreadTakesWait_TimeExpires'),
        test_filter.TestFilter(
            'nplb', 'SbWindowCreateTest.SunnyDayDefault'),
        test_filter.TestFilter(
            'nplb', 'SbWindowCreateTest.SunnyDayDefaultSet'),
        test_filter.TestFilter(
            'nplb', 'SbWindowGetPlatformHandleTest.SunnyDay'),
        test_filter.TestFilter(
            'nplb', 'SbWindowGetSizeTest.SunnyDay'),
        test_filter.TestFilter(
            'nplb', 'SbPlayerTest.SunnyDay'),
        # test fails when built with GCC 4.9, issue was fixed in later versions of GCC
        test_filter.TestFilter(
            'nplb', 'SbAlignTest.AlignAsStackVariable'),
        # tests fail also on x86
        test_filter.TestFilter(
            'nplb', 'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/1'),
        test_filter.TestFilter(
            'nplb', 'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/1'),
        test_filter.TestFilter(
            'nplb', 'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/1'),
        # there are no test cases in this test
        test_filter.TestFilter(
            'starboard_platform_tests', test_filter.FILTER_ALL),
        # there are no test cases in this test
        test_filter.TestFilter(
            'nplb_blitter_pixel_tests', test_filter.FILTER_ALL),
        # test fails on x64 also
        test_filter.TestFilter(
            'net_unittests', 'HostResolverImplDnsTest.DnsTaskUnspec'),
        # we don't have proper procedure for running this test
        test_filter.TestFilter(
            'web_platform_tests', test_filter.FILTER_ALL),
    ]


def CreatePlatformConfig():
  try:
    return PlatformConfig('creator-ci20x11-gcc-4-9')
  except RuntimeError as e:
    logging.critical(e)
    return None
