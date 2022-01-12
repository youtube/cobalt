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


class TestFilters(object):
  """Starboard Linux platform test filters."""

  def GetTestFilters(self):
    filters = []

    has_cdm = os.path.isfile(
        os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'ce_cdm', 'cdm',
                     'include', 'cdm.h'))

    for target, tests in self._FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)

    if has_cdm:
      return filters

    # Filter the drm related tests, as ce_cdm is not present.
    for target, tests in self._DRM_RELATED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  _DRM_RELATED_TESTS = {
      'nplb': [
          'SbDrmTest.AnySupportedKeySystems',
          'SbMediaCanPlayMimeAndKeySystem.AnySupportedKeySystems',
      ],
  }

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
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/1',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/1',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/1',
    ])
  # TODO: Re-enable once tests or infra fixed.
  _FILTERED_TESTS['nplb'].extend([
      'SbSocketAddressTypes/SbSocketBindTest.RainyDayBadInterface/1',
      'SbSocketAddressTypes/PairSbSocketGetLocalAddressTest.SunnyDayConnected/1',
      'SbSocketAddressTypes/PairSbSocketIsConnectedAndIdleTest.SunnyDay/1',
      'SbSocketAddressTypes/PairSbSocketIsConnectedTest.SunnyDay/1',
      'SbSocketAddressTypes/PairSbSocketReceiveFromTest.SunnyDay/1',
      'SbSocketAddressTypes/SbSocketResolveTest.Localhost/1',
      'SbSocketAddressTypes/SbSocketResolveTest.SunnyDayFiltered/1',
      'SbSocketAddressTypes/PairSbSocketSendToTest.RainyDaySendToClosedSocket/1',
      'SbSocketAddressTypes/PairSbSocketSendToTest.RainyDaySendToSocketUntilBlocking/1',
      'SbSocketAddressTypes/PairSbSocketSendToTest.RainyDaySendToSocketConnectionReset/1',
      'SbSocketAddressTypes/PairSbSocketWaiterWaitTest.SunnyDay/1',
      'SbSocketAddressTypes/PairSbSocketWaiterWaitTest.SunnyDayAlreadyReady/1',
      'SbSocketAddressTypes/PairSbSocketWaiterWaitTimedTest.SunnyDay/1',
      'SbSocketAddressTypes/PairSbSocketWrapperTest.SunnyDay/1',
  ])

  # pylint: enable=line-too-long


def CreateTestFilters():
  return TestFilters()
