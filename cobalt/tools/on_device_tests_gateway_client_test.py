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
"""Tests for cobalt.tools.on_device_tests_gateway_client."""

import argparse
import json
import os
import shutil
import sys
import tempfile
import types
from typing import Any, Mapping
import unittest
from unittest import mock

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
if _REPO_ROOT not in sys.path:
  sys.path.insert(0, _REPO_ROOT)

if 'grpc' not in sys.modules:
  fake_grpc = types.ModuleType('grpc')
  fake_grpc.RpcError = Exception
  fake_grpc.insecure_channel = mock.MagicMock()
  sys.modules['grpc'] = fake_grpc

if 'on_device_tests_gateway_pb2' not in sys.modules:
  fake_pb2 = types.ModuleType('on_device_tests_gateway_pb2')
  fake_pb2.OnDeviceTestsCommand = mock.MagicMock()
  fake_pb2.OnDeviceTestsWatchCommand = mock.MagicMock()
  sys.modules['on_device_tests_gateway_pb2'] = fake_pb2

if 'on_device_tests_gateway_pb2_grpc' not in sys.modules:
  fake_pb2_grpc = types.ModuleType('on_device_tests_gateway_pb2_grpc')
  fake_pb2_grpc.on_device_tests_gatewayStub = mock.MagicMock()
  sys.modules['on_device_tests_gateway_pb2_grpc'] = fake_pb2_grpc

# pylint: disable=wrong-import-position
from cobalt.tools import on_device_tests_gateway_client
from cobalt.tools.on_device_tests_gateway_client import process_test_requests
# pylint: enable=wrong-import-position


class ProcessTestRequestsTest(unittest.TestCase):
  """Tests for process_test_requests and sharding behavior."""

  def setUp(self):
    self.filter_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.filter_dir)

  def _write_filter(self, filename: str, data: Mapping[str, Any]) -> None:
    filepath = os.path.join(self.filter_dir, filename)
    with open(filepath, 'w', encoding='utf-8', newline='') as f:
      json.dump(data, f)

  def _make_args(self, targets: Any, **overrides: Any) -> argparse.Namespace:
    defaults = {
        'test_type':
            'unit_test',
        'device_family':
            'android',
        'targets':
            json.dumps(targets),
        'filter_json_dir':
            self.filter_dir,
        'gtest_filter':
            None,
        'gcs_archive_path':
            '/bigstore/test-artifacts/android-arm',
        'gcs_result_path':
            'gs://cobalt-unittest-storage/results/arm/123/1',
        'dimensions':
            json.dumps({
                'device_type': 'arm_devices',
                'device_pool': 'shared',
            }),
        'job_timeout_sec':
            '2700',
        'test_timeout_sec':
            '1800',
        'start_timeout_sec':
            '900',
        'retry_level':
            None,
        'test_attempts':
            '1',
        'artifact_name':
            'Cobalt.apk',
        'cobalt_path':
            None,
    }
    defaults.update(overrides)
    return argparse.Namespace(**defaults)

  def test_unsharded_targets_keep_root_result_path_and_clean_test_args(self):
    args = self._make_args([
        'base:base_unittests',
        {
            'target': 'url:url_unittests',
            'shards': 1,
        },
        {
            'target': 'net:net_unittests',
            'test_attempts': '2',
        },
    ])

    requests = process_test_requests(args)

    self.assertEqual(len(requests), 3)
    self.assertEqual(requests[0]['test_target'], 'base:base_unittests')
    self.assertEqual(requests[0]['test_type'], 'unit_test')
    self.assertEqual(
        requests[0]['test_args'].count('test_attempts=1'),
        1,
    )
    self.assertIn(
        'gcs_result_path=gs://cobalt-unittest-storage/results/arm/123/1',
        requests[0]['params'],
    )
    self.assertNotIn('gcs_delete_before_upload=true', requests[0]['params'])
    self.assertEqual(
        requests[0]['test_cmd_args'],
        [('command_line_args='
          '--gtest_output=xml:/sdcard/Download/base_unittests_testoutput.xml '
          '--gtest_filter=* --single-process-tests --num-retries=0')],
    )

    self.assertEqual(requests[1]['test_target'], 'url:url_unittests')
    self.assertEqual(
        requests[1]['test_args'].count('test_attempts=1'),
        1,
    )
    self.assertIn(
        'gcs_result_path=gs://cobalt-unittest-storage/results/arm/123/1',
        requests[1]['params'],
    )

    self.assertEqual(requests[2]['test_target'], 'net:net_unittests')
    self.assertNotIn('test_attempts=1', requests[2]['test_args'])
    self.assertEqual(
        requests[2]['test_args'].count('test_attempts=2'),
        1,
    )
    self.assertIn('gcs_delete_before_upload=true', requests[2]['params'])

  def test_sharded_cobalt_browsertests_initial_run(self):
    self._write_filter(
        'cobalt_browsertests_filter.json',
        {'failing_tests': ['DisabledSuite.TestA', 'DisabledSuite.TestB']},
    )
    args = self._make_args([{
        'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
        'shards': 4,
    }])

    requests = process_test_requests(args)

    self.assertEqual(len(requests), 4)
    for shard_idx, req in enumerate(requests):
      self.assertEqual(req['test_type'], 'browser_test')
      expected_label = ('cobalt/testing/browser_tests:cobalt_browsertests '
                        f'(shard {shard_idx} of 4)')
      self.assertEqual(req['test_target'], expected_label)
      self.assertEqual(
          req['test_target'].split('/')[-1],
          f'browser_tests:cobalt_browsertests (shard {shard_idx} of 4)',
      )
      self.assertIn(
          'host_deps=/bigstore/test-artifacts/android-arm/'
          'cobalt_browsertests_host_deps.tar.gz',
          req['files'],
      )
      self.assertIn(
          'gcs_result_path=gs://cobalt-unittest-storage/results/arm/123/1/'
          f'shard_{shard_idx}',
          req['params'],
      )
      self.assertIn(
          'gcs_result_filename=cobalt_browsertests_testoutput.xml',
          req['params'],
      )
      cmd_line = req['test_cmd_args'][0]
      self.assertIn(
          '--gtest_filter=-DisabledSuite.TestA:DisabledSuite.TestB',
          cmd_line,
      )
      self.assertIn(f'--test-launcher-shard-index={shard_idx}', cmd_line)
      self.assertIn('--test-launcher-total-shards=4', cmd_line)
      self.assertNotIn('--gtest_shard_index', cmd_line)

  def test_sharded_target_skipped_when_global_filter_skips_all(self):
    self._write_filter('cobalt_browsertests_filter.json',
                       {'failing_tests': ['*']})
    args = self._make_args([{
        'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
        'shards': 4,
    }])

    requests = process_test_requests(args)

    self.assertEqual(requests, [])

  def test_sharded_browsertests_retry_skips_passed_and_disables_resharding(
      self):
    # Base platform filter disables KnownBad.Test.
    self._write_filter('cobalt_browsertests_filter.json',
                       {'failing_tests': ['KnownBad.Test']})
    # Shards 0 and 1 passed on attempt 1.
    self._write_filter('cobalt_browsertests_0_filter.json', {
        'failing_tests': ['*'],
        'tests_to_run': []
    })
    self._write_filter('cobalt_browsertests_1_filter.json', {
        'failing_tests': ['*'],
        'tests_to_run': []
    })
    # Shard 2 failed two specific tests on attempt 1.
    self._write_filter(
        'cobalt_browsertests_2_filter.json',
        {
            'failing_tests': [],
            'tests_to_run': ['Suite.FlakyOne', 'Suite.FlakyTwo'],
        },
    )
    # Shard 3 crashed on attempt 1 (no shard_3 filter written).

    args = self._make_args(
        [{
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'shards': 4,
        }],
        gcs_result_path='gs://cobalt-unittest-storage/results/arm/123/2/',
    )

    requests = process_test_requests(args)

    self.assertEqual(len(requests), 2)

    shard2_req = requests[0]
    self.assertEqual(
        shard2_req['test_target'],
        'cobalt/testing/browser_tests:cobalt_browsertests (shard 2 of 4)',
    )
    self.assertIn(
        'gcs_result_path=gs://cobalt-unittest-storage/results/arm/123/2/'
        'shard_2',
        shard2_req['params'],
    )
    shard2_cmd = shard2_req['test_cmd_args'][0]
    self.assertIn('--gtest_filter=Suite.FlakyOne:Suite.FlakyTwo', shard2_cmd)
    self.assertNotIn('--test-launcher-shard-index', shard2_cmd)
    self.assertNotIn('--test-launcher-total-shards', shard2_cmd)

    shard3_req = requests[1]
    self.assertEqual(
        shard3_req['test_target'],
        'cobalt/testing/browser_tests:cobalt_browsertests (shard 3 of 4)',
    )
    self.assertIn(
        'gcs_result_path=gs://cobalt-unittest-storage/results/arm/123/2/'
        'shard_3',
        shard3_req['params'],
    )
    shard3_cmd = shard3_req['test_cmd_args'][0]
    self.assertIn('--gtest_filter=-KnownBad.Test', shard3_cmd)
    self.assertIn('--test-launcher-shard-index=3', shard3_cmd)
    self.assertIn('--test-launcher-total-shards=4', shard3_cmd)

  def test_sharded_browser_test_supports_max_shards(self):
    args = self._make_args([{
        'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
        'shards': 10,
    }])

    requests = process_test_requests(args)

    self.assertEqual(len(requests), 10)
    for shard_idx, req in enumerate(requests):
      self.assertEqual(req['test_type'], 'browser_test')
      self.assertEqual(
          req['test_target'],
          'cobalt/testing/browser_tests:cobalt_browsertests '
          f'(shard {shard_idx} of 10)',
      )
      self.assertIn(
          'gcs_result_path=gs://cobalt-unittest-storage/results/arm/123/1/'
          f'shard_{shard_idx}',
          req['params'],
      )
      cmd_line = req['test_cmd_args'][0]
      self.assertIn(f'--test-launcher-shard-index={shard_idx}', cmd_line)
      self.assertIn('--test-launcher-total-shards=10', cmd_line)

    explicit_browser_args = self._make_args(
        [{
            'target': 'cobalt/testing/browser_tests:custom_browser_suite',
            'shards': 4,
        }],
        test_type='browser_test',
    )
    explicit_requests = process_test_requests(explicit_browser_args)
    self.assertEqual(len(explicit_requests), 4)
    self.assertEqual(explicit_requests[0]['test_type'], 'browser_test')
    self.assertIn('--test-launcher-shard-index=0',
                  explicit_requests[0]['test_cmd_args'][0])
    self.assertIn('--test-launcher-total-shards=4',
                  explicit_requests[0]['test_cmd_args'][0])

  def test_sharded_non_browser_test_raises_value_error(self):
    unit_args = self._make_args([{
        'target': 'cobalt:cobalt_unittests',
        'shards': 2,
    }])
    with self.assertRaises(ValueError):
      process_test_requests(unit_args)

    e2e_args = self._make_args(
        [{
            'target': 'e2e_suite',
            'shards': 2,
        }],
        test_type='e2e_test',
        cobalt_path='test-artifacts/android-arm',
    )
    with self.assertRaises(ValueError):
      process_test_requests(e2e_args)

    with self.assertRaises(ValueError):
      # pylint: disable=protected-access
      on_device_tests_gateway_client._shard_cmd_args('unit_test', 0, 2)

  def test_invalid_shards_raises_value_error(self):
    for bad_shards in (0, -2, 11, 100, 1.5, 'invalid', True, False):
      with self.subTest(shards=bad_shards):
        args = self._make_args([{
            'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
            'shards': bad_shards,
        }])
        with self.assertRaises(ValueError):
          process_test_requests(args)

  def test_main_triggers_gateway_client_with_sharded_requests(self):
    targets_json = json.dumps([{
        'target': 'cobalt/testing/browser_tests:cobalt_browsertests',
        'shards': 4,
    }])
    argv = [
        'on_device_tests_gateway_client.py',
        '--token',
        'fake-token',
        'trigger',
        '--test_type',
        'unit_test',
        '--device_family',
        'android',
        '--targets',
        targets_json,
        '--filter_json_dir',
        self.filter_dir,
        '--label',
        'chrobalt_on_device_test',
        '--dimensions',
        json.dumps({
            'device_type': 'arm_devices',
            'device_pool': 'shared',
        }),
        '--gcs_archive_path',
        '/bigstore/artifacts/android-arm',
        '--gcs_result_path',
        'gs://cobalt-unittest-storage/results/arm/999/1',
    ]
    with mock.patch.object(sys, 'argv', argv), mock.patch.object(
        on_device_tests_gateway_client,
        'OnDeviceTestsGatewayClient') as mock_client_cls:
      exit_code = on_device_tests_gateway_client.main()

    self.assertEqual(exit_code, 0)
    mock_client = mock_client_cls.return_value
    mock_client.run_trigger_command.assert_called_once()
    token, labels, test_requests = mock_client.run_trigger_command.call_args[0]
    self.assertEqual(token, 'fake-token')
    self.assertEqual(labels, ['chrobalt_on_device_test'])
    self.assertEqual(len(test_requests), 4)


if __name__ == '__main__':
  unittest.main()
