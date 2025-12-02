# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Evergreen X64 Platform Test Filters."""

import os

from starboard.evergreen.shared import test_filters as shared_test_filters
from starboard.tools import paths
from starboard.tools.testing import test_filter

# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'nplb': [
        'MultiplePlayerTests/*/*sintel_329_ec3_dmp*',
        'MultiplePlayerTests/*/*sintel_381_ac3_dmp*',
        'SbPlayerGetAudioConfigurationTests/*audio_sintel_329_ec3_dmp_*',
        'SbPlayerGetAudioConfigurationTests/*audio_sintel_381_ac3_dmp_*',
        'SbPlayerGetMediaTimeTests/*audio_sintel_329_ec3_dmp_*',
        'SbPlayerGetMediaTimeTests/*audio_sintel_381_ac3_dmp_*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.WriteSingleBatch/audio_sintel_329_ec3_dmp_*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.WriteSingleBatch/audio_sintel_381_ac3_dmp_*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.WriteMultipleBatches/audio_sintel_329_ec3_dmp_*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.WriteMultipleBatches/audio_sintel_381_ac3_dmp_*',
    ],
}
# pylint: enable=line-too-long


def CreateTestFilters():
  return EvergreenX64TestFilters()


class EvergreenX64TestFilters(shared_test_filters.TestFilters):
  """Starboard Evergreen X64 Platform Test Filters."""

  def GetTestFilters(self):
    filters = super().GetTestFilters()

    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)

    # Remove the exclusion filter on SbDrmTest.AnySupportedKeySystems.
    # Generally, children of linux/shared do not support widevine, but children
    # of linux/x64x11 do, if the content decryption module is present.
    has_cdm = os.path.isfile(
        os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'cdm', 'cdm',
                     'include', 'content_decryption_module.h'))

    if not has_cdm:
      return filters

    def IsAllowedTest(test_filter_item):
      return (test_filter_item.target_name == 'nplb' and
              test_filter_item.test_name == 'SbDrmTest.AnySupportedKeySystems')

    return [
        test_filter_item for test_filter_item in filters
        if not IsAllowedTest(test_filter_item)
    ]
