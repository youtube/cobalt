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
"""Starboard win-win32 Platform Test Filters."""

from starboard.shared.win32 import test_filters as shared_test_filters
from starboard.tools.testing import test_filter


# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'nplb': [
        # These tests hang too long on Windows.
        'PosixSocketSendTest.RainyDaySendToClosedSocket',
        'PosixSocketSendtoTest.RainyDaySendToClosedSocket',

        # Currently frequently flaky on Windows
        'SbConditionVariableWaitTimedTest.FLAKY_SunnyDayAutoInit',
        'PosixConditionVariableWaitTimedTest.FLAKY_SunnyDayAutoInit',

        # This single test takes >15 minutes.
        'SbPlayerTest.MultiPlayer',
        # This test fails on win-win32 devel builds, because the compiler
        # performs an optimization that defeats the SB_C_NOINLINE 'noinline'
        # attribute.
        'SbSystemGetStackTest.SunnyDayStackDirection',

        # These tests are failing. Enable them once they're supported.
        'MultiplePlayerTests/*beneath_the_canopy_137_avc_dmp*',
        'SbMediaSetAudioWriteDurationTests/SbMediaSetAudioWriteDurationTest.WriteContinuedLimitedInput*',
        'SbPlayerWriteSampleTests/SbPlayerWriteSampleTest.SecondaryPlayerTest/*',

        # Failures tracked by b/256160416.
        'PosixSocketHints/PosixSocketResolveTest.SunnyDayHints/family_inet6_*',
        'SbSystemGetPathTest.ReturnsRequiredPaths',
        'SbPlayerGetAudioConfigurationTests/*_video_beneath_the_canopy_137_avc_dmp_output_decode_to_texture_*',
        'SbPlayerGetMediaTimeTests/*_video_beneath_the_canopy_137_avc_dmp_output_decode_to_texture_*',
        'SbPlayerWriteSampleTests/*_video_beneath_the_canopy_137_avc_dmp_output_decode_to_texture_*',
        'SbSocketAddressTypes/SbSocketBindTest.RainyDayBadInterface/type_ipv6_filter_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDayDestination/type_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceForDestination/type_ipv6',
        'SbSocketAddressTypes/SbSocketGetInterfaceAddressTest.SunnyDaySourceNotLoopback/type_ipv6',
        'SbSocketAddressTypes/SbSocketResolveTest.SunnyDayFiltered/filter_ipv6_type_ipv6',
        'SbSocketAddressTypes/SbSocketSetOptionsTest.RainyDayInvalidSocket/type_ipv4',
        'SbSocketAddressTypes/SbSocketSetOptionsTest.RainyDayInvalidSocket/type_ipv6',
        # Flakiness is tracked in b/278276779.
        'Semaphore.ThreadTakesWait_TimeExpires',
        # Failure tracked by b/287666606.
        'VerticalVideoTests/VerticalVideoTest.WriteSamples*',

        # Enable once verified on the platform.
        'SbMediaCanPlayMimeAndKeySystem.MinimumSupport',
    ],
    'player_filter_tests': [
        # These tests fail on our VMs for win-win32 builds due to missing
        # or non functioning system video decoders.
        'VideoDecoderTests/VideoDecoderTest.*/beneath_the_canopy_137_avc_dmp_DecodeToTexture*',
        'VideoDecoderTests/VideoDecoderTest.*/black_test_avc_1080p_30to60_fps_dmp_DecodeToTexture*',

        # PlayerComponentsTests fail on our VMs. Preroll callback is always not called in
        # 5 seconds, which causes timeout error.
        'PlayerComponentsTests/*',
    ],

    # TODO: b/330792170 - Fix remaining failing win32 tests.
    'net_unittests': [
        'CookieMonsterTest.InheritCreationDate',
        'FileStreamTest.UseFileHandle',  # Fails on github but not locally.
        'UDPSocketTest.PartialRecv',
        'UDPSocketTest.ConnectRandomBind',
        'UDPSocketTest.ConnectFail',
        'UDPSocketTest.LimitConnectFail',
        'UDPSocketTest.ReadWithSocketOptimization',  # This test crashes.
        'EmbeddedTestServerTestInstantiation/EmbeddedTestServerTest.ConnectionListenerComplete/0',
        'PartitionedCookiesURLRequestHttpJobTest.PrivacyMode/0',
        'TransportSecurityStateTest.MatchesCase2',
        'TransportSecurityStateTest.DecodePreloadedSingle',
        'TransportSecurityStateTest.DecodePreloadedMultipleMix',
        'TransportSecurityStateTest.DomainNameOddities',
        'TransportSecurityStateTest.DecodePreloadedMultiplePrefix',
        'TransportSecurityStateTest.HstsHostBypassList',
    ],
}


def CreateTestFilters():
  return WinWin32TestFilters()


class WinWin32TestFilters(shared_test_filters.TestFilters):
  """Starboard win-win32 Platform Test Filters."""

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    filters = super().GetTestFilters()
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters
