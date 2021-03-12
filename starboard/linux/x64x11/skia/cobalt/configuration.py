# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Linux x64x11 Skia Cobalt configuration."""

import os

from starboard.linux.shared.cobalt import configuration as shared_configuration
from starboard.tools.testing import test_filter


class CobaltLinuxX64X11SkiaConfiguration(
    shared_configuration.CobaltLinuxConfiguration):
  """Starboard Linux x64x11 Skia Cobalt configuration."""

  def GetTestFilters(self):
    filters = super(CobaltLinuxX64X11SkiaConfiguration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # A map of failing or crashing tests per target.
  __FILTERED_TESTS = {
      # Tracked by b/181270083
      'renderer_test': ['PixelTest.RectWithRoundedCornersOnSolidColor',],
  }
