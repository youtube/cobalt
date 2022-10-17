# Copyright 2017-2021 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Android x86 Cobalt configuration."""

from starboard.android.shared.cobalt import configuration
from starboard.tools.testing import test_filter

_FILTERED_TESTS = {
    'renderer_test': [
        'PixelTest.YUV422UYVYImageScaledUpSupport',
        'PixelTest.YUV422UYVYImageScaledAndTranslated'
    ],
    'layout_tests': [('CSS3FontsLayoutTests/Layout.Test/'
                      '5_2_use_first_available_listed_font_family'),
                     ('CSS3FontsLayoutTests/Layout.Test/'
                      '5_2_use_specified_font_family_if_available'),
                     ('CSS3FontsLayoutTests/Layout.Test/'
                      'synthetic_bolding_should_not_occur_on_bold_font'),
                     ('CSS3FontsLayoutTests/Layout.Test/'
                      'synthetic_bolding_should_occur_on_non_bold_font')]
}


class CobaltAndroidArmConfiguration(configuration.CobaltAndroidConfiguration):
  """Starboard Android x86 Cobalt configuration."""

  def GetTestFilters(self):
    filters = super(CobaltAndroidArmConfiguration, self).GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
