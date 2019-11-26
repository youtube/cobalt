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
"""Starboard Raspberry Pi 0 platform configuration."""

from starboard.raspi.shared import gyp_configuration as shared_configuration
from starboard.tools.testing import test_filter


class Raspi0PlatformConfig(shared_configuration.RaspiPlatformConfig):

  def __init__(self, platform, sabi_json_path=None):
    super(Raspi0PlatformConfig, self).__init__(
        platform, sabi_json_path=sabi_json_path)

  def GetVariables(self, config_name):
    variables = super(Raspi0PlatformConfig, self).GetVariables(config_name)
    variables.update({
        'javascript_engine': 'v8',
        'cobalt_enable_jit': 1,
    })
    return variables

  def GetTestFilters(self):
    filters = super(Raspi0PlatformConfig, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  __FILTERED_TESTS = {
      'nplb': [
          # TODO: debug these failures.
          'SbPlayerTest.MultiPlayer',  # crashes
      ],
      'player_filter_tests': [
          # The implementation for the raspberry pi 0 is incomplete and not
          # meant to be a reference implementation. As such we will not repair
          # these failing tests for now.
          'VideoDecoderTests/VideoDecoderTest.DecodeFullGOP/0',
          'VideoDecoderTests/VideoDecoderTest.HoldFramesUntilFull/0',
          'VideoDecoderTests/VideoDecoderTest.MultipleInputs/0',
          'VideoDecoderTests/VideoDecoderTest.Preroll/0',
      ]
  }


def CreatePlatformConfig():
  return Raspi0PlatformConfig(
      'raspi-0', sabi_json_path='starboard/sabi/arm/hardfp/v6zk/sabi.json')
