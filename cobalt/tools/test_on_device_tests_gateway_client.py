#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Tests for on_device_tests_gateway_client.py.
"""Tests for the on-device tests gateway client."""

import argparse
import json
import os
import sys
import unittest
from unittest import mock

# Add the directory to sys.path to import the client
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
# pylint: disable=wrong-import-position
import on_device_tests_gateway_client


class TestOnDeviceTestsGatewayClient(unittest.TestCase):
  """Unit tests for the gateway client."""

  def test_process_test_requests_browser_test(self):
    """Verifies that browser_test requests are correctly formed."""
    docker_tag = (
        'us-west1-docker.pkg.dev/ytlr-image-hosting/integration-test-images/'
        'browser-test-experimental:latest')
    args = argparse.Namespace(
        test_type='browser_test',
        targets=json.dumps([{
            'target': 'testing/container/browser_tests:cobalt_browser_test',
            'test_attempts': '3'
        }]),
        dimensions=json.dumps({
            'device_type': 'arm_devices',
            'device_pool': 'maneki'
        }),
        job_timeout_sec='2700',
        test_timeout_sec='1800',
        start_timeout_sec='900',
        retry_level='ERROR',
        test_attempts='3',
        docker_tag=docker_tag,
        gcs_result_path='gs://results/android-arm/123/1',
        device_family='android',
        label=['test_label'],
        artifact_name='Cobalt.apk',
        cobalt_path='some/path',
        filter_json_dir='/tmp/filters')

    # Mock os.path.exists to avoid filter file checks
    with mock.patch('os.path.exists', return_value=False):
      # pylint: disable=protected-access
      requests = on_device_tests_gateway_client._process_test_requests(args)

    self.assertEqual(len(requests), 1)
    req = requests[0]
    self.assertEqual(req['test_type'], 'e2e_test')
    self.assertEqual(req['test_target'],
                     'testing/container/browser_tests:cobalt_browser_test')
    self.assertIn(f'docker_tag={docker_tag}', req['params'])
    self.assertIn('gcs_result_path=gs://results/android-arm/123/1',
                  req['params'])

  def test_piper_compatibility(self):
    """Verifies that the request matches what Google3 expects."""
    docker_tag = (
        'us-west1-docker.pkg.dev/ytlr-image-hosting/integration-test-images/'
        'browser-test-experimental:latest')
    args = argparse.Namespace(
        test_type='browser_test',
        targets=json.dumps([{
            'target': 'testing/container/browser_tests:cobalt_browser_test'
        }]),
        dimensions=json.dumps({
            'device_type': 'arm_devices',
            'device_pool': 'maneki'
        }),
        job_timeout_sec='2700',
        test_timeout_sec='1800',
        start_timeout_sec='900',
        retry_level='ERROR',
        test_attempts='3',
        docker_tag=docker_tag,
        gcs_result_path='gs://results',
        device_family='android',
        label=[],
        artifact_name='Cobalt.apk',
        cobalt_path='some/path',
        filter_json_dir='/tmp/filters')

    with mock.patch('os.path.exists', return_value=False):
      # pylint: disable=protected-access
      requests = on_device_tests_gateway_client._process_test_requests(args)

    req = requests[0]
    params_dict = dict(p.split('=', 1) for p in req['params'])
    self.assertIn('docker_tag', params_dict)
    self.assertEqual(params_dict['docker_tag'], docker_tag)
    self.assertEqual(req['test_target'],
                     'testing/container/browser_tests:cobalt_browser_test')

  def test_process_test_requests_unit_test_attempts(self):
    """Verifies that test_attempts from targets JSON overrides cmdline."""
    args = argparse.Namespace(
        test_type='unit_test',
        targets=json.dumps([{
            'target': 'cobalt/test:my_test',
            'test_attempts': '5'
        }]),
        dimensions=json.dumps({
            'device_type': 'arm_devices',
            'device_pool': 'maneki'
        }),
        job_timeout_sec='2700',
        test_timeout_sec='1800',
        start_timeout_sec='900',
        retry_level='ERROR',
        test_attempts='3',
        docker_tag=None,
        gcs_result_path='gs://results',
        device_family='android',
        label=['test_label'],
        artifact_name='Cobalt.apk',
        gcs_archive_path='gs://archive',
        filter_json_dir='/tmp/filters')

    with mock.patch('os.path.exists', return_value=False):
      # pylint: disable=protected-access
      requests = on_device_tests_gateway_client._process_test_requests(args)

    self.assertEqual(len(requests), 1)
    self.assertIn('test_attempts=5', requests[0]['test_args'])
    self.assertNotIn('test_attempts=3', requests[0]['test_args'])


if __name__ == '__main__':
  unittest.main()
