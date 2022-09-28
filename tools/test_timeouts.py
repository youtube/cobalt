#!/usr/bin/python
#
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
"""Test timeout utilities for build workers."""

import worker_tools

_DEFAULT_UNIT_TEST_TIMEOUT_MIN = 90
_DEFAULT_PERFORMANCE_TEST_TIMEOUT_MIN = 30
_DEFAULT_ENDURANCE_TEST_TIMEOUT_MIN = 550
_DEFAULT_BLACK_BOX_TEST_TIMEOUT_MIN = 30
_DEFAULT_EVERGREEN_TEST_TIMEOUT_MIN = 105
_DEFAULT_INTEGRATION_TEST_TIMEOUT_MIN = 25


class _PlatformTestTimeouts(object):
  """Store test times for a particular platform."""

  def __init__(
      self,
      unit_test_timeout_min=_DEFAULT_UNIT_TEST_TIMEOUT_MIN,
      perf_test_timeout_min=_DEFAULT_PERFORMANCE_TEST_TIMEOUT_MIN,
      endurance_test_timeout_min=_DEFAULT_ENDURANCE_TEST_TIMEOUT_MIN,
      black_box_test_timeout_min=_DEFAULT_BLACK_BOX_TEST_TIMEOUT_MIN,
      evergreen_test_timeout_min=_DEFAULT_EVERGREEN_TEST_TIMEOUT_MIN,
      integration_test_timeout_min=_DEFAULT_INTEGRATION_TEST_TIMEOUT_MIN):
    """Values of None are allowed, and they will use defaults."""
    self.unit_test_timeout_min = unit_test_timeout_min
    self.perf_test_timeout_min = perf_test_timeout_min
    self.endurance_test_timeout_min = endurance_test_timeout_min
    self.black_box_test_timeout_min = black_box_test_timeout_min
    self.evergreen_test_timeout_min = evergreen_test_timeout_min
    self.integration_test_timeout_min = integration_test_timeout_min

  def GetTestTimeoutMinutes(self, test_type):
    if test_type == worker_tools.TestTypes.UNIT_TEST:
      return self.unit_test_timeout_min
    if test_type == worker_tools.TestTypes.PERFORMANCE_TEST:
      return self.perf_test_timeout_min
    if test_type == worker_tools.TestTypes.ENDURANCE_TEST:
      return self.endurance_test_timeout_min
    if test_type == worker_tools.TestTypes.BLACK_BOX_TEST:
      return self.black_box_test_timeout_min
    if test_type == worker_tools.TestTypes.EVERGREEN_TEST:
      return self.evergreen_test_timeout_min
    if test_type == worker_tools.TestTypes.INTEGRATION_TEST:
      return self.integration_test_timeout_min
    raise ValueError('Unknown test type: %s' % test_type)


# These test timeouts are enough for the majority of platforms.
# If a platform needs more or substantially less, then can be added to
# _PLATFORM_TO_TEST_TIMEOUTS
_DEFAULT_TEST_TIMEOUTS = _PlatformTestTimeouts(
    _DEFAULT_UNIT_TEST_TIMEOUT_MIN, _DEFAULT_PERFORMANCE_TEST_TIMEOUT_MIN,
    _DEFAULT_ENDURANCE_TEST_TIMEOUT_MIN, _DEFAULT_BLACK_BOX_TEST_TIMEOUT_MIN,
    _DEFAULT_EVERGREEN_TEST_TIMEOUT_MIN, _DEFAULT_INTEGRATION_TEST_TIMEOUT_MIN)

# Associates a base platform name like android or xb1 with test times.
_PLATFORM_TO_TEST_TIMEOUTS = {
    'evergreen-arm':
        _PlatformTestTimeouts(
            unit_test_timeout_min=120, black_box_test_timeout_min=40),
    'android':
        _PlatformTestTimeouts(
            unit_test_timeout_min=85,
            perf_test_timeout_min=35,
            black_box_test_timeout_min=20),
}


def GetPlatformTestTimeoutMinutesInt(platform, test_type):
  for base_platform, times in _PLATFORM_TO_TEST_TIMEOUTS.items():
    if platform.startswith(base_platform):
      return times.GetTestTimeoutMinutes(test_type)
  return _DEFAULT_TEST_TIMEOUTS.GetTestTimeoutMinutes(test_type)
