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
"""Starboard Android arm64 Cobalt configuration."""

from starboard.android.shared.cobalt import configuration
from starboard.tools.testing import test_filter


class CobaltAndroidARM64Configuration(configuration.CobaltAndroidConfiguration):
  """Starboard Android arm64 Cobalt configuration."""

  def GetTestFilters(self):
    filters = super(CobaltAndroidARM64Configuration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  # pylint:disable=line-too-long
  # A map of failing or crashing tests per target
  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'net_unittests': [
          # NetlinkConnection::SendRequest() fails, send() failed, errno=13.
          'DialHttpServerTest.*',
      ],
  }
