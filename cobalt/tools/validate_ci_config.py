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
"""Validates GitHub Actions configuration files for consistency."""

import json
import os
import unittest

_MAX_SHARDS = 10


class TestCIConfigConsistency(unittest.TestCase):
  """Checks consistency between test flags and targets in platform configs."""

  def setUp(self):
    self.config_dir = os.path.join(
        os.path.dirname(__file__), '..', '..', '.github', 'config')
    self.targets_dir = os.path.join(
        os.path.dirname(__file__), '..', 'build', 'testing', 'targets')

  def _simulate_ci_logic(self, config):
    """Simulates the 'if' conditions in main.yaml.

    Returns a dict mapping job names to their expected execution state.
    """
    test_browser = config.get('test_browser', False)
    # browser_test_targets_json check proxy:
    has_targets = len(config.get('browser_test_targets', [])) > 0

    # Centralized run_browser_tests output:
    run_browser_tests = test_browser and has_targets
    run_browser_tests_str = 'true' if run_browser_tests else 'false'

    # Simulated Job logic from main.yaml:
    # browser-test-on-host:
    # if: needs.initialize.outputs.run_browser_tests == 'true'
    browser_test_on_host_runs = run_browser_tests_str == 'true'

    # browser-test-results:
    # if: always() && needs.initialize.outputs.run_browser_tests == 'true'
    browser_test_results_runs = run_browser_tests_str == 'true'

    # validate-test-result: (part related to browser tests)
    # if: ... || (needs.initialize.outputs.run_browser_tests == 'true' && ...)
    validate_browser_logic_runs = run_browser_tests_str == 'true'

    return {
        'run-browser-tests-output': run_browser_tests_str,
        'browser-test-on-host': browser_test_on_host_runs,
        'browser-test-results': browser_test_results_runs,
        'validate-browser-logic': validate_browser_logic_runs
    }

  def test_browser_test_skip_logic(self):
    """Verifies consistency between test flags and jobs."""
    for filename in os.listdir(self.config_dir):
      if not filename.endswith('.json') or 'infra' in filename:
        continue

      path = os.path.join(self.config_dir, filename)
      with open(path, 'r', encoding='utf-8') as f:
        try:
          config = json.load(f)
        except json.JSONDecodeError:
          continue

        test_browser = config.get('test_browser', False)
        ci_state = self._simulate_ci_logic(config)

        if not test_browser:
          self.assertFalse(
              ci_state['browser-test-on-host'],
              f'{filename}: browser-test-on-host should NOT run when '
              'test_browser is false')
        else:
          # Verify that if it IS true, the jobs are enabled if targets exist.
          has_targets = len(config.get('browser_test_targets', [])) > 0
          if has_targets:
            self.assertTrue(
                ci_state['browser-test-on-host'],
                f'{filename}: browser-test-on-host SHOULD run when '
                'test_browser is true and targets exist')

  def test_android_arm_explicitly(self):
    """Checks the specific problematic config that caused CI hangs."""
    path = os.path.join(self.config_dir, 'android-arm.json')
    if not os.path.exists(path):
      return
    with open(path, 'r', encoding='utf-8') as f:
      config = json.load(f)
      test_browser = config.get('test_browser', False)
      self.assertFalse(test_browser,
                       'android-arm must have test_browser: false')

      ci_state = self._simulate_ci_logic(config)
      self.assertFalse(
          ci_state['browser-test-results'],
          'Browser test results job must be skipped for android-arm')

  def _validate_test_target_shards(self, entries, source_label):
    """Validates optional per-target 'shards' fields in test_targets.json."""
    self.assertIsInstance(
        entries, list, f'{source_label}: test_targets.json root must be a list')
    for idx, entry in enumerate(entries):
      if not isinstance(entry, dict) or 'shards' not in entry:
        continue
      shards = entry['shards']
      target = entry.get('target', '')
      target_label = target or 'unnamed'
      item_label = f'{source_label}[{idx}] ({target_label})'
      self.assertTrue(
          isinstance(shards, int) and not isinstance(shards, bool),
          f'{item_label}: shards must be an int, got {shards!r}',
      )
      self.assertTrue(
          1 <= shards <= _MAX_SHARDS,
          f'{item_label}: shards must be between 1 and {_MAX_SHARDS}, '
          f'got {shards}',
      )
      self.assertEqual(
          entry.get('runs_on'),
          'device',
          f'{item_label}: shards is only supported for runs_on="device"',
      )
      target_name = target.split(':')[-1] if isinstance(target, str) else ''
      executable = entry.get('executable')
      is_android_apk_gtest = (
          entry.get('test_type') == 'gtest' and isinstance(executable, str) and
          executable.endswith('.apk') and 'junit' not in target_name.lower())
      self.assertTrue(
          'cobalt_browsertests' in target_name or is_android_apk_gtest,
          f'{item_label}: shards is only supported on cobalt_browsertests or '
          'non-JUnit Android APK GTest targets',
      )

  def test_test_targets_shards_consistency(self):
    """Verifies per-target shards config across all test_targets.json files."""
    if not os.path.exists(self.targets_dir):
      return
    checked_files = 0
    for platform_dir in sorted(os.listdir(self.targets_dir)):
      path = os.path.join(self.targets_dir, platform_dir, 'test_targets.json')
      if not os.path.isfile(path):
        continue
      with open(path, 'r', encoding='utf-8') as f:
        entries = json.load(f)
      self._validate_test_target_shards(entries,
                                        f'{platform_dir}/test_targets.json')
      checked_files += 1
    self.assertGreater(checked_files, 0, 'Expected test_targets.json files')

  def test_test_target_shards_validator_rejects_invalid_entries(self):
    """Verifies shard validator catches invalid types, bounds, and targets."""
    self._validate_test_target_shards(
        [
            {
                'TODO': 'metadata entry without target'
            },
            {
                'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
                'runs_on': 'device',
                'test_type': 'gtest',
                'shards': 1,
            },
            {
                'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
                'runs_on': 'device',
                'test_type': 'gtest',
                'shards': 10,
            },
            {
                'target': 'base:base_unittests',
                'executable': 'out/android-arm_devel/base_unittests-debug.apk',
                'runs_on': 'device',
                'test_type': 'gtest',
                'shards': 2,
            },
        ],
        'valid_sample',
    )

    invalid_entries = [
        {
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'runs_on': 'device',
            'shards': True,
        },
        {
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'runs_on': 'device',
            'shards': 1.5,
        },
        {
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'runs_on': 'device',
            'shards': '4',
        },
        {
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'runs_on': 'device',
            'shards': 0,
        },
        {
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'runs_on': 'device',
            'shards': 11,
        },
        {
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'runs_on': 'host',
            'shards': 4,
        },
        {
            'target': 'base:base_unittests',
            'executable': 'out/android-arm_devel/base_unittests-debug.apk',
            'runs_on': 'host',
            'test_type': 'gtest',
            'shards': 2,
        },
        {
            'target': 'starboard/nplb:nplb_loader',
            'executable': 'out/evergreen-arm-hardfp-rdk_devel/nplb_loader.py',
            'runs_on': 'device',
            'test_type': 'gtest',
            'shards': 2,
        },
        {
            'target': 'cobalt:cobalt_unittests',
            'runs_on': 'device',
            'shards': 4,
        },
    ]
    for bad_entry in invalid_entries:
      with self.subTest(entry=bad_entry):
        with self.assertRaises(AssertionError):
          self._validate_test_target_shards([bad_entry], 'invalid_sample')


if __name__ == '__main__':
  unittest.main()
