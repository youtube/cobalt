#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Unit tests for filter_expected_tests.py."""

import unittest

from filter_expected_tests import filter_expected_tests


class TestFilterExpectedTests(unittest.TestCase):
  """Unit tests for filter_expected_tests."""

  def setUp(self):
    self.sample_targets = [
        {
            'TODO': 'comment block'
        },
        {
            'target': 'base:base_unittests',
            'runs_on': 'host',
            'test_type': 'gtest',
        },
        {
            'target': 'starboard:nplb',
            'runs_on': 'device',
            'test_type': 'gtest',
        },
        {
            'target': 'junit:some_junit',
            'runs_on': 'device',
            'test_type': 'junit',
        },
        {
            'target': 'e2e:browse_test',
            'runs_on': 'device',
            'test_type': 'e2e_test',
        },
        {
            'target': 'yts:yts_finch',
            'runs_on': 'device',
            'test_type': 'yts_finch_test',
        },
        {
            'target': 'yts:playback',
            'runs_on': 'device',
            'test_type': 'yts_playback_test',
        },
        {
            'target': 'no_type:target',
            'runs_on': 'device',
        },
    ]

  def test_filter_gtest_device_only(self):
    run_flags = {'gtest_device': True, 'gtest_host': False}
    result = filter_expected_tests(self.sample_targets, run_flags)
    targets = [t['target'] for t in result]
    self.assertEqual(targets, ['starboard:nplb', 'junit:some_junit'])

  def test_filter_gtest_host_only(self):
    run_flags = {'gtest_device': False, 'gtest_host': True}
    result = filter_expected_tests(self.sample_targets, run_flags)
    targets = [t['target'] for t in result]
    self.assertEqual(targets, ['base:base_unittests'])

  def test_filter_internal_tests(self):
    run_flags = {
        'e2e_test': True,
        'yts_finch_test': True,
        'yts_playback_test': False,
    }
    result = filter_expected_tests(self.sample_targets, run_flags)
    targets = [t['target'] for t in result]
    # YTS tests are excluded from expected tests because they do not output
    # JUnit XMLs.
    self.assertEqual(targets, ['e2e:browse_test'])


if __name__ == '__main__':
  unittest.main()
