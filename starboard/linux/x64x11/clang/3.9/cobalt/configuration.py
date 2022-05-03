# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Linux x64x11 Clang 3.9 Cobalt configuration."""

from starboard.linux.shared.cobalt import configuration as shared_configuration
from starboard.tools.testing import test_filter


class CobaltLinuxX64X11Clang39Configuration(
    shared_configuration.CobaltLinuxConfiguration):
  """Starboard Linux x64x11 Clang 3.9 Cobalt configuration."""

  def GetTestFilters(self):
    filters = super(CobaltLinuxX64X11Clang39Configuration,
                    self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # A map of failing or crashing tests per target.
  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'zip_unittests': [
          'ZipReaderTest.ExtractToFileAsync_RegularFile',
      ],
      # Tracked by b/226999079.
      'renderer_test': [
          'LottieCoveragePixelTest*skottie_linear_wipe_effect_json',
          'LottieCoveragePixelTest*skottie_matte_blendmode_json',
          'LottieCoveragePixelTest*skottie_shift_channels_effect_json',
          'LottieCoveragePixelTest*skottie_3d_2planes_json',
          'LottieCoveragePixelTest*skottie_venetianblinds_effect_json',
      ],
  }
