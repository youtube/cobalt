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
import pathlib
import unittest

_MAX_SHARDS = 10
_RUNNER_JOB_FILTERS = (
    ('test_on_device', 'odt', (('gtest', 'device'),)),
    ('test_on_host', 'always', (('gtest', 'host'), ('junit', 'host'))),
    ('test_e2e', 'odt', (('e2e_test', 'device'),)),
    ('test_yts', 'odt', (('yts_test', 'device'),)),
    ('test_yts_playback', 'yts_playback', (('yts_playback_test', 'device'),)),
    ('test_yts_wpt', 'wpt', (('yts_wpt_test', 'device'),)),
    ('test_yts_finch', 'odt', (('yts_finch_test', 'device'),)),
)
_DEVICE_TYPE_FLAGS = {
    'gtest': 'test_on_device',
    'e2e_test': 'test_e2e',
    'yts_test': 'test_yts',
    'yts_playback_test': 'test_yts_playback',
    'yts_wpt_test': 'test_yts_wpt',
    'yts_finch_test': 'test_yts_finch',
}
_ODT_FLAGS = tuple(_DEVICE_TYPE_FLAGS.values())


def _trigger_active(kind, is_odt_enabled, has_playback_label, is_wpt_trigger):
  """Returns whether a runner job's trigger condition is active."""
  if kind == 'always':
    return True
  if kind == 'odt':
    return is_odt_enabled
  if kind == 'yts_playback':
    return has_playback_label
  if kind == 'wpt':
    return is_wpt_trigger
  raise ValueError(f'Unknown trigger kind: {kind}')


def _compute_expected_tests(cfg, entries, is_odt_enabled, has_playback_label,
                            is_wpt_trigger):
  """Mirrors main.yaml's expected_tests_json jq filter."""
  run_device = cfg.get('test_on_device') is True and is_odt_enabled
  run_host = cfg.get('test_on_host') is True
  run_e2e = cfg.get('test_e2e') is True and is_odt_enabled
  run_yts = cfg.get('test_yts') is True and is_odt_enabled
  run_playback = (cfg.get('test_yts_playback') is True and has_playback_label)
  run_wpt = cfg.get('test_yts_wpt') is True and is_wpt_trigger
  run_finch = cfg.get('test_yts_finch') is True and is_odt_enabled
  device_flags = {
      'gtest': run_device,
      'e2e_test': run_e2e,
      'yts_test': run_yts,
      'yts_playback_test': run_playback,
      'yts_wpt_test': run_wpt,
      'yts_finch_test': run_finch,
  }
  expected = []
  for e in entries:
    if not isinstance(e, dict) or e.get('target') is None:
      continue
    if e.get('process_results') is False:
      continue
    runs_on = e.get('runs_on')
    test_type = e.get('test_type')
    if runs_on == 'device' and device_flags.get(test_type, False):
      expected.append(e)
    elif runs_on == 'host' and run_host and test_type in ('gtest', 'junit'):
      expected.append(e)
  return expected


def _runner_test_results_targets(cfg, entries, is_odt_enabled,
                                 has_playback_label, is_wpt_trigger):
  """Computes the union of targets produced by runner jobs for test-results."""
  selected = set()
  for flag, kind, pairs in _RUNNER_JOB_FILTERS:
    if cfg.get(flag) is not True:
      continue
    if not _trigger_active(kind, is_odt_enabled, has_playback_label,
                           is_wpt_trigger):
      continue
    for e in entries:
      if not isinstance(e, dict) or e.get('target') is None:
        continue
      if e.get('process_results') is False:
        continue
      if (e.get('test_type'), e.get('runs_on')) in pairs:
        selected.add(e['target'])
  return selected


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

  def _iter_config_targets(self):
    """Yields (filename, cfg, plat, entries) for non-infra CI configs."""
    for name in sorted(os.listdir(self.config_dir)):
      if not name.endswith('.json') or 'infra' in name:
        continue
      cfg = json.loads(
          (pathlib.Path(self.config_dir) / name).read_text(encoding='utf-8'))
      for plat in cfg.get('platforms') or [None]:
        tp = (
            pathlib.Path(self.targets_dir) / (plat or '') / 'test_targets.json')
        entries = (
            json.loads(tp.read_text(
                encoding='utf-8')) if plat and tp.is_file() else [])
        yield name, cfg, plat, entries

  def test_expected_tests_matches_runner_jobs(self):
    """Verifies expected_tests parity with runner jobs across all configs."""
    checked = 0
    scenarios = (
        (True, False, True),
        (False, False, False),
        (False, True, False),
        (True, True, True),
    )
    for name, cfg, plat, entries in self._iter_config_targets():
      checked += 1
      for odt, playback, wpt in scenarios:
        expected = _compute_expected_tests(cfg, entries, odt, playback, wpt)
        expected_set = {e['target'] for e in expected}
        runner_set = _runner_test_results_targets(cfg, entries, odt, playback,
                                                  wpt)
        self.assertEqual(
            len(expected),
            len(expected_set),
            f'{name}/{plat}: duplicate targets in expected_tests',
        )
        self.assertEqual(
            expected_set ^ runner_set,
            set(),
            f'{name}/{plat} (odt={odt}, pb={playback}, wpt={wpt}): '
            f'expected vs runner mismatch: '
            f'only_expected={expected_set - runner_set}, '
            f'only_runner={runner_set - expected_set}',
        )
        for entry in expected:
          self.assertNotEqual(entry.get('process_results'), False)
          self.assertIsNotNone(entry.get('test_type'))
          self.assertIn(entry.get('runs_on'), ('device', 'host'))
          self.assertNotIn(entry.get('test_type'), ('browsertest', 'webtest'))
      if plat == 'android-arm':
        expected = _compute_expected_tests(cfg, entries, True, False, True)
        expected_targets = {e['target'] for e in expected}
        self.assertNotIn(
            'cobalt/memory_instrumentation:cobalt_memory_instrumentation_'
            'unittests',
            expected_targets,
        )
        excluded = {
            e['target'] for e in entries if isinstance(e, dict) and
            e.get('target') and e.get('process_results') is False
        }
        self.assertTrue(excluded)
        self.assertEqual(expected_targets & excluded, set())
    self.assertGreater(checked, 0)

  def test_expected_tests_excludes_phantoms_and_process_results_false(self):
    """Verifies expected_tests excludes untyped, browsertest, and gated runs."""
    cfg = {
        'test_on_device': True,
        'test_on_host': True,
        'test_e2e': True,
        'test_yts_playback': True,
        'test_yts_wpt': True,
        'test_browser': True,
    }
    entries = [
        {
            'target': 'a:dev_gtest',
            'runs_on': 'device',
            'test_type': 'gtest'
        },
        {
            'target': 'a:host_gtest',
            'runs_on': 'host',
            'test_type': 'gtest'
        },
        {
            'target': 'a:host_junit',
            'runs_on': 'host',
            'test_type': 'junit'
        },
        {
            'target': 'a:e2e',
            'runs_on': 'device',
            'test_type': 'e2e_test'
        },
        {
            'target': 'a:yts_pb',
            'runs_on': 'device',
            'test_type': 'yts_playback_test',
        },
        {
            'target': 'a:yts_wpt',
            'runs_on': 'device',
            'test_type': 'yts_wpt_test',
        },
        {
            'target': 'a:untyped',
            'runs_on': None,
            'test_type': None
        },
        {
            'target': 'a:null_runs_on',
            'runs_on': None,
            'test_type': 'gtest'
        },
        {
            'target': 'a:browser',
            'runs_on': 'host',
            'test_type': 'browsertest',
        },
        {
            'target': 'a:web',
            'runs_on': 'host',
            'test_type': 'webtest'
        },
        {
            'target': 'a:skip_proc',
            'runs_on': 'device',
            'test_type': 'gtest',
            'process_results': False,
        },
    ]
    exp_pr = {
        e['target']
        for e in _compute_expected_tests(cfg, entries, True, False, True)
    }
    self.assertEqual(
        exp_pr,
        {'a:dev_gtest', 'a:host_gtest', 'a:host_junit', 'a:e2e', 'a:yts_wpt'},
    )
    self.assertEqual(
        exp_pr,
        _runner_test_results_targets(cfg, entries, True, False, True),
    )
    exp_draft_pb = {
        e['target']
        for e in _compute_expected_tests(cfg, entries, False, True, False)
    }
    self.assertEqual(exp_draft_pb, {'a:host_gtest', 'a:host_junit', 'a:yts_pb'})
    self.assertEqual(
        exp_draft_pb,
        _runner_test_results_targets(cfg, entries, False, True, False),
    )

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
      self.assertIn(
          'cobalt_browsertests',
          target_name,
          f'{item_label}: shards is only supported on cobalt_browsertests '
          'targets',
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
            'target': 'cobalt:cobalt_unittests',
            'runs_on': 'device',
            'shards': 4,
        },
    ]
    for bad_entry in invalid_entries:
      with self.subTest(entry=bad_entry):
        with self.assertRaises(AssertionError):
          self._validate_test_target_shards([bad_entry], 'invalid_sample')

  def _validate_odt_config(self, cfg, entries, label):
    """Validates ODT artifact, device family, and active target flags."""
    if any(cfg.get(f) is True for f in _ODT_FLAGS):
      art = cfg.get('test_artifact_name')
      self.assertTrue(isinstance(art, str) and bool(art), f'{label}: artifact')
      fam = cfg.get('test_device_family')
      self.assertTrue(
          isinstance(fam, str) and bool(fam), f'{label}: device_family')
      self.assertTrue(
          entries, f'{label}: ODT flags set but test_targets.json '
          'missing/empty')
    for e in entries:
      if not isinstance(e, dict) or e.get('process_results') is False:
        continue
      test_type = e.get('test_type')
      if e.get('runs_on') == 'device' and test_type in _DEVICE_TYPE_FLAGS:
        flag = _DEVICE_TYPE_FLAGS[test_type]
        self.assertIs(cfg.get(flag), True, f'{label}: {flag}')

  def test_odt_configs_and_validators(self):
    """Verifies ODT configs and negative cases for artifact/family/flags."""
    for name, cfg, _, entries in self._iter_config_targets():
      self._validate_odt_config(cfg, entries, name)
    e2e = {'target': 'x:e2e', 'runs_on': 'device', 'test_type': 'e2e_test'}
    yts = {'target': 'x:yts', 'runs_on': 'device', 'test_type': 'yts_test'}
    odt_base = {
        'test_on_device': True,
        'test_artifact_name': 'Cobalt.apk',
        'test_device_family': 'android',
    }
    for cfg, entries in (
        ({
            'test_e2e': True,
            'test_device_family': 'android',
        }, []),
        ({
            'test_on_device': True,
            'test_artifact_name': '',
            'test_device_family': 'android',
        }, []),
        ({
            'test_on_device': True,
            'test_artifact_name': 'Cobalt.apk',
        }, []),
        ({
            'test_on_device': True,
            'test_artifact_name': 'Cobalt.apk',
            'test_device_family': '',
        }, []),
        ({}, [e2e]),
        (odt_base, [yts]),
        (odt_base, []),
    ):
      with self.assertRaises(AssertionError):
        self._validate_odt_config(cfg, entries, 'bad')
    self._validate_odt_config({}, [{**e2e, 'process_results': False}], 'ok')
    self._validate_odt_config(odt_base, [{
        **yts,
        'process_results': False,
    }], 'ok')


if __name__ == '__main__':
  unittest.main()
