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
"""Starboard Linux Platform Test Filters."""

import os

from starboard.tools import paths
from starboard.tools.testing import test_filter


# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'player_filter_tests': [
        # libdav1d crashes when fed invalid data
        'VideoDecoderTests/VideoDecoderTest.*Invalid*',
    ],
}
_FILTERED_TESTS['nplb'] = []
# Conditionally disables tests that require ipv6
if os.getenv('IPV6_AVAILABLE', '1') == '0':
  _FILTERED_TESTS['nplb'].extend([
      'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/type_ipv6',
      'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/type_ipv6',
      'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/type_ipv6',
      'SbSocketAddressTypes/SbSocketBindTest.RainyDayBadInterface/type_ipv6_filter_ipv6',
      'SbSocketAddressTypes/PairSbSocketGetLocalAddressTest.SunnyDayConnected/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketIsConnectedAndIdleTest.SunnyDay/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketIsConnectedTest.SunnyDay/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketReceiveFromTest.SunnyDay/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/SbSocketResolveTest.Localhost/filter_ipv6_type_ipv6',
      'SbSocketAddressTypes/SbSocketResolveTest.SunnyDayFiltered/filter_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketSendToTest.RainyDaySendToClosedSocket/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketSendToTest.RainyDaySendToSocketUntilBlocking/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketSendToTest.RainyDaySendToSocketConnectionReset/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketWaiterWaitTest.SunnyDay/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketWaiterWaitTest.SunnyDayAlreadyReady/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketWaiterWaitTimedTest.SunnyDay/type_ipv6_type_ipv6',
      'SbSocketAddressTypes/PairSbSocketWrapperTest.SunnyDay/type_ipv6_type_ipv6',
  ])

# pylint: enable=line-too-long


class TestFilters(object):
  """Starboard Linux platform test filters."""

  def GetTestFilters(self):
    filters = []

    has_cdm = os.path.isfile(
        os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'ce_cdm', 'cdm',
                     'include', 'cdm.h'))

    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)

    if has_cdm:
      return filters

    # Filter the drm related tests, as ce_cdm is not present.
    for target, tests in self._DRM_RELATED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  _DRM_RELATED_TESTS = {
      'nplb': [
          'SbDrmTest.AnySupportedKeySystems',
          'SbMediaCanPlayMimeAndKeySystem.AnySupportedKeySystems',
      ],
  }


def CreateTestFilters():
  return TestFilters()
