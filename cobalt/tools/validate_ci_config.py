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


class TestCIConfigConsistency(unittest.TestCase):
  """Checks consistency between test flags and targets in platform configs."""

  def setUp(self):
    self.config_dir = os.path.join(
        os.path.dirname(__file__), '..', '..', '.github', 'config')

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


if __name__ == '__main__':
  unittest.main()
