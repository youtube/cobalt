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
"""Starboard Android shared platform configuration for gyp_cobalt."""

from __future__ import print_function

import imp
import os

import gyp_utils
import sdk_utils
from starboard.build.platform_configuration import PlatformConfiguration
from starboard.tools.testing import test_filter


class PlatformConfig(PlatformConfiguration):
  """Starboard Android platform configuration."""

  # TODO: make ASAN work with NDK tools and enable it by default
  def __init__(self, platform, android_abi, asan_enabled_by_default=False):
    super(PlatformConfig, self).__init__(platform, asan_enabled_by_default)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

    self.android_abi = android_abi
    self.ndk_tools = sdk_utils.GetToolsPath(android_abi)
    sdk_utils.InstallSdkIfNeeded(android_abi)

    self.host_compiler_environment = gyp_utils.GetHostCompilerEnvironment()
    self.android_home = sdk_utils.GetSdkPath()
    self.android_ndk_home = sdk_utils.GetNdkPath()

    print('Using Android SDK at {}'.format(self.android_home))
    print('Using Android NDK at {}'.format(self.android_ndk_home))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,qtcreator_ninja'

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(
        configuration, use_clang=1)
    variables.update({
        'ANDROID_HOME': self.android_home,
        'NDK_HOME': self.android_ndk_home,
        'ANDROID_ABI': self.android_abi,
        'enable_remote_debugging': 0,
        'include_path_platform_deploy_gypi':
            'starboard/android/shared/platform_deploy.gypi',
    })
    return variables

  def GetGeneratorVariables(self, configuration):
    _ = configuration
    generator_variables = {
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetEnvironmentVariables(self):
    env_variables = sdk_utils.GetEnvironmentVariables(self.android_abi)
    env_variables.update(self.host_compiler_environment)
    # Android builds tend to consume significantly more memory than the
    # default settings permit, so cap this at 1 in order to avoid build
    # issues.  Without this, 32GB machines end up getting automatically
    # configured to run 5 at a time, which can be too much for at least
    # android-arm64_debug.
    # TODO: Eventually replace this with something more robust, like an
    #       implementation of the abstract toolchain for Android.
    env_variables.update({'GYP_LINK_CONCURRENCY': '1'})

    return env_variables

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(os.path.join(
        os.path.dirname(__file__), 'launcher.py'))
    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

  def GetTestFilters(self):
    filters = super(PlatformConfig, self).GetTestFilters()
    for target, tests in self._FAILING_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # A map of failing or crashing tests per target.
  _FAILING_TESTS = {
      'nplb': [
          'SbAudioSinkTest.AllFramesConsumed',
          'SbAudioSinkTest.SomeFramesConsumed',
          'SbCharIsSignedTest.SunnyDay',

          # SbPlayer tests fail because VideoWindow is not set in tests.
          'SbPlayerTest.Audioless',
          'SbPlayerTest.SunnyDay',

          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDayDestination/0',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceForDestination/0',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceForDestination/1',
          'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest'
          '.SunnyDaySourceNotLoopback/0',
          'SbSocketBindTest.SunnyDayLocalInterface',
          'SbSocketGetLocalAddressTest.SunnyDayBoundSpecified',
          'SpeechRecognizerTest.StartIsCalledMultipleTimes',
          'SpeechRecognizerTest.StartRecognizerWith10MaxAlternatives',
          'SpeechRecognizerTest.StartRecognizerWithContinuousRecognition',
          'SpeechRecognizerTest.StartRecognizerWithInterimResults',
          'SpeechRecognizerTest.StartTestSunnyDay',

          # The following fail on MiBox
          'SbAudioSinkTest.ContinuousAppend',
          'SbAudioSinkTest.MultipleAppendAndConsume',
          'SbAudioSinkTest.Pause',
          # The following reboot on MiBox
          'SpeechRecognizerTest.DestroyRecognizerWithoutStopping',
          'SpeechRecognizerTest.StartWithInvalidSpeechRecognizer',
          'SpeechRecognizerTest.StopIsCalledMultipleTimes',

      ],
  }
