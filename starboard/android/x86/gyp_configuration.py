# Copyright 2016-2021 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Android x86 platform build configuration."""

from starboard.android.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter


def CreatePlatformConfig():
  return Androidx86Configuration(
      'android-x86',
      'x86',
      sabi_json_path='starboard/sabi/x86/sabi-v{sb_api_version}.json')


class Androidx86Configuration(shared_configuration.AndroidConfiguration):

  def GetTestFilters(self):
    filters = super(Androidx86Configuration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # A map of failing or crashing tests per target
  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'nplb': [
          'SbAccessibilityTest.CallSetCaptionsEnabled',
          'SbAccessibilityTest.GetCaptionSettingsReturnIsValid',
          'SbAudioSinkTest.*',
          'SbMediaCanPlayMimeAndKeySystem.*',
          'SbMicrophoneCloseTest.*',
          'SbMicrophoneOpenTest.*',
          'SbMicrophoneReadTest.*',
          'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.*',
          'SbMediaSetAudioWriteDurationTests/SbMediaSetAudioWriteDurationTest'
          '.WriteContinuedLimitedInput/*',
          'SbMediaSetAudioWriteDurationTests/SbMediaSetAudioWriteDurationTest'
          '.WriteLimitedInput/*',
      ],
      'player_filter_tests': [
          'AudioDecoderTests/*',
          'VideoDecoderTests/*',

          'PlayerComponentsTests/*',
      ],
  }
