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

from starboard.tools import paths
from starboard.evergreen.shared import test_filters as shared_test_filters


def CreateTestFilters():
  return EvergreenX64TestFilters()


class EvergreenX64TestFilters(shared_test_filters.TestFilters):
  """Starboard Evergreen X64 Platform Test Filters."""

  def GetTestFilters(self):
    filters = super(EvergreenX64TestFilters, self).GetTestFilters()
    # Remove the exclusion filter on SbDrmTest.AnySupportedKeySystems.
    # Generally, children of linux/shared do not support widevine, but children
    # of linux/x64x11 do, if the content decryption module is present.

    has_cdm = os.path.isfile(
        os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'cdm', 'cdm',
                     'include', 'content_decryption_module.h'))

    if not has_cdm:
      return filters

    test_filters = []
    for test_filter in filters:
      if (test_filter.target_name == 'nplb' and
          test_filter.test_name == 'SbDrmTest.AnySupportedKeySystems'):
        continue
      test_filters.append(test_filter)
    return test_filters
