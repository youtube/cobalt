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
"""Unit tests for on_device_tests_gateway_client.py."""

import argparse
import sys
import unittest
from unittest import mock

sys.modules.setdefault('grpc', mock.MagicMock())
sys.modules.setdefault('on_device_tests_gateway_pb2', mock.MagicMock())
sys.modules.setdefault('on_device_tests_gateway_pb2_grpc', mock.MagicMock())

from cobalt.tools import on_device_tests_gateway_client  # pylint: disable=wrong-import-position
# pylint: disable=protected-access


class OnDeviceTestsGatewayClientTest(unittest.TestCase):
  """Unit tests for _process_test_requests."""

  def _make_args(self, **kwargs) -> argparse.Namespace:
    defaults = {
        'test_type':
            'yts_test',
        'device_family':
            'android',
        'targets':
            '["testing/maneki/cobalt/kimono_tests:yts_dom_test"]',
        'filter_json_dir':
            None,
        'gcs_result_path':
            'gs://bucket/results',
        'retry_level':
            None,
        'dimensions':
            '{"device_type": "AndroidDevice", "device_pool": "maneki"}',
        'job_timeout_sec':
            '2700',
        'test_timeout_sec':
            '1800',
        'start_timeout_sec':
            '900',
        'gtest_filter':
            None,
        'gcs_archive_path':
            None,
        'cobalt_path':
            'cobalt-artifacts/main/1/android-arm',
        'artifact_name':
            'Cobalt.apk',
    }
    defaults.update(kwargs)
    return argparse.Namespace(**defaults)

  def test_yts_test_uses_build_apk_and_uploader_plugin(self):
    args = self._make_args(test_type='yts_test')
    requests = on_device_tests_gateway_client._process_test_requests(args)
    self.assertEqual(len(requests), 1)
    req = requests[0]
    self.assertEqual(req['test_type'], 'yts_test')
    self.assertIn(
        'build_apk=/bigstore/cobalt-artifacts/main/1/android-arm/Cobalt.apk',
        req['files'])
    self.assertIn(
        ('client_plugin_jar='
         f'{on_device_tests_gateway_client._GCS_UPLOADER_PLUGIN_JAR}'),
        req['files'])
    self.assertIn('app=dev.cobalt.coat', req['params'])
    self.assertIn('gcs_log_filename=yts_dom_test_log.txt', req['params'])

  def test_yts_wpt_test_uses_cobalt_path_and_preserves_wpt_plugins(self):
    args = self._make_args(
        test_type='yts_wpt_test',
        targets='["testing/container/wpt:yts_wpt_test_ci_android_arm_devices"]',
    )
    requests = on_device_tests_gateway_client._process_test_requests(args)
    self.assertEqual(len(requests), 1)
    req = requests[0]
    self.assertEqual(req['test_type'], 'e2e_test')
    self.assertIn(
        'cobalt_path=/bigstore/cobalt-artifacts/main/1/android-arm/Cobalt.apk',
        req['files'])
    self.assertFalse(any(f.startswith('build_apk=') for f in req['files']))
    plugin_files = [
        f for f in req['files'] if f.startswith('client_plugin_jar=')
    ]
    self.assertEqual(len(plugin_files), 1)
    self.assertIn(
        on_device_tests_gateway_client._WPT_ARCHIVE_DOWNLOADER_PLUGIN_JAR,
        plugin_files[0])
    self.assertIn(on_device_tests_gateway_client._WPT_RESULT_PARSER_PLUGIN_JAR,
                  plugin_files[0])
    self.assertIn(on_device_tests_gateway_client._GCS_UPLOADER_PLUGIN_JAR,
                  plugin_files[0])
    self.assertIn('gcs_result_filename=results/results.json', req['params'])
    self.assertIn(
        'gcs_log_filename=yts_wpt_test_ci_android_arm_devices_cobalt.log',
        req['params'])
    self.assertIn(
        ('dimension_host_name='
         f'{on_device_tests_gateway_client._WPT_DEFAULT_HOST_NAME_REGEX}'),
        req['test_args'])


if __name__ == '__main__':
  unittest.main()
