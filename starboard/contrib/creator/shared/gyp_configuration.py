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
"""Starboard Creator Ci20 platform configuration."""

import logging
import os
import sys

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools.testing import test_filter


class CreatorConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard ci20 platform configuration."""

  def __init__(self, platform):
    super(CreatorConfiguration, self).__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def _GetCi20Home(self):
    try:
      ci20_home = os.environ['CI20_HOME']
    except KeyError:
      logging.critical('ci20 builds require the `CI20_HOME\' '
                       'environment variable to be set.')
      sys.exit(1)
    return ci20_home

  def GetVariables(self, config_name):
    relative_sysroot = os.path.join('sysroot')
    sysroot = os.path.join(self.ci20_home, relative_sysroot)

    if not os.path.isdir(sysroot):
      logging.critical(
          'ci20 builds require $CI20_HOME/%s to be a valid directory.',
          relative_sysroot)
      sys.exit(1)
    variables = super(CreatorConfiguration, self).GetVariables(
        config_name, use_clang=1)
    variables.update({
        'sysroot': sysroot,
    })

    return variables

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    # Use launcher.py from src/starboard/linux/shared/
    linux_shared = os.path.join(
        os.path.dirname(__file__), '..', '..', '..', 'linux', 'shared')
    return linux_shared

  def GetGeneratorVariables(self, config_name):
    del config_name
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetEnvironmentVariables(self):
    self.ci20_home = self._GetCi20Home()
    self.host_compiler_environment = build.GetHostCompilerEnvironment(
        clang.GetClangSpecification(), False)
    env_variables = self.host_compiler_environment
    env_variables = {
        'CC': self.host_compiler_environment['CC_host'],
        'CXX': self.host_compiler_environment['CXX_host'],
        'CC_host': 'gcc',
        'CXX_host': 'g++',
        'LD_host': 'g++',
        'ARFLAGS_host': 'rcs',
        'ARTHINFLAGS_host': 'rcsT',
    }

    return env_variables

  def GetTestFilters(self):
    filters = super(CreatorConfiguration, self).GetTestFilters()
    filters.extend([
        # test is disabled on x64
        test_filter.TestFilter('bindings_test',
                               ('GlobalInterfaceBindingsTest.'
                                'PropertiesAndOperationsAreOwnProperties')),
        # tests miss the defined delay
        test_filter.TestFilter('nplb',
                               'SbConditionVariableWaitTimedTest.SunnyDay'),
        test_filter.TestFilter(
            'nplb', 'SbConditionVariableWaitTimedTest.SunnyDayAutoInit'),
        # tests sometimes miss the threshold of 10ms
        test_filter.TestFilter(
            'nplb', 'Semaphore.ThreadTakesWait_PutBeforeTimeExpires'),
        test_filter.TestFilter('nplb', 'RWLock.HoldsLockForTime'),
        # tests sometimes miss the threshold of 5ms
        test_filter.TestFilter('nplb', 'Semaphore.ThreadTakesWait_TimeExpires'),
        test_filter.TestFilter('nplb', 'SbWindowCreateTest.SunnyDayDefault'),
        test_filter.TestFilter('nplb', 'SbWindowCreateTest.SunnyDayDefaultSet'),
        test_filter.TestFilter('nplb',
                               'SbWindowGetPlatformHandleTest.SunnyDay'),
        test_filter.TestFilter('nplb', 'SbWindowGetSizeTest.SunnyDay'),
        test_filter.TestFilter('nplb', 'SbPlayerTest.SunnyDay'),
        # tests fail also on x86
        test_filter.TestFilter(
            'nplb',
            ('SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.'
             'SunnyDayDestination/1')
        ),
        test_filter.TestFilter(
            'nplb',
            ('SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.'
             'SunnyDaySourceForDestination/1')
        ),
        test_filter.TestFilter(
            'nplb',
            ('SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.'
             'SunnyDaySourceNotLoopback/1')
        ),
        # there are no test cases in this test
        test_filter.TestFilter('nplb_blitter_pixel_tests',
                               test_filter.FILTER_ALL),
        # test fails on x64 also
        test_filter.TestFilter('net_unittests',
                               'HostResolverImplDnsTest.DnsTaskUnspec'),
        # we don't have proper procedure for running this test
        test_filter.TestFilter('web_platform_tests', test_filter.FILTER_ALL),
    ])
    return filters
