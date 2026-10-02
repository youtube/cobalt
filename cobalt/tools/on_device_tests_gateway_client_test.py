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
"""Tests for on_device_tests_gateway_client."""

import pathlib
import tempfile
import unittest
from unittest.mock import MagicMock

from cobalt.tools.on_device_tests_gateway_client import (
    OnDeviceTestsGatewayClient,
    _generate_junit_xml_report,
)


class MockResponseLine:

  def __init__(self, text):
    self.response = text


class OnDeviceTestsGatewayClientTest(unittest.TestCase):

  def test_generate_junit_xml_report_pass(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      out_dir = pathlib.Path(temp_dir)
      _generate_junit_xml_report('my_target', 'PASS', 'Log snippet', out_dir)
      xml_file = out_dir / 'my_target_testoutput.xml'
      self.assertTrue(xml_file.exists())
      content = xml_file.read_text(encoding='utf-8')
      self.assertIn('<testsuite name="my_target"', content)
      self.assertIn('errors="0"', content)
      self.assertNotIn('<error', content)

  def test_generate_junit_xml_report_fail(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      out_dir = pathlib.Path(temp_dir)
      _generate_junit_xml_report('failed_target', 'FAIL', 'Failure details',
                                 out_dir)
      xml_file = out_dir / 'failed_target_testoutput.xml'
      self.assertTrue(xml_file.exists())
      content = xml_file.read_text(encoding='utf-8')
      self.assertIn('<testsuite name="failed_target"', content)
      self.assertIn('errors="1"', content)
      self.assertIn('<error message="Internal test status: FAIL">', content)
      self.assertIn('Failure details', content)

  def test_run_trigger_command_tracks_multiple_targets(self):
    client = OnDeviceTestsGatewayClient()
    client.stub = MagicMock()
    client.stub.exec_command.return_value = [
        MockResponseLine('Starting session for target_1 and target_2'),
        MockResponseLine('Target target_1: Job result: PASS'),
        MockResponseLine('Target target_2: Job result: FAIL'),
    ]

    test_requests = [
        {
            'test_target': 'suite:target_1'
        },
        {
            'test_target': 'suite:target_2'
        },
    ]

    with tempfile.TemporaryDirectory() as temp_dir:
      all_passed = client.run_trigger_command(
          token='dummy',
          labels=['label1'],
          test_requests=test_requests,
          local_result_dir=temp_dir,
      )

      self.assertFalse(all_passed)
      t1_xml = pathlib.Path(temp_dir) / 'target_1_testoutput.xml'
      t2_xml = pathlib.Path(temp_dir) / 'target_2_testoutput.xml'

      self.assertTrue(t1_xml.exists())
      self.assertTrue(t2_xml.exists())

      self.assertIn('errors="0"', t1_xml.read_text(encoding='utf-8'))
      self.assertIn('errors="1"', t2_xml.read_text(encoding='utf-8'))

  def test_run_trigger_command_all_passed(self):
    client = OnDeviceTestsGatewayClient()
    client.stub = MagicMock()
    client.stub.exec_command.return_value = [
        MockResponseLine('Target target_pass: Status: COMPLETED, Result: PASS'),
    ]

    test_requests = [
        {
            'test_target': 'suite:target_pass'
        },
    ]

    with tempfile.TemporaryDirectory() as temp_dir:
      all_passed = client.run_trigger_command(
          token='dummy',
          labels=['label1'],
          test_requests=test_requests,
          local_result_dir=temp_dir,
      )

      self.assertTrue(all_passed)
      t_xml = pathlib.Path(temp_dir) / 'target_pass_testoutput.xml'
      self.assertTrue(t_xml.exists())
      self.assertIn('errors="0"', t_xml.read_text(encoding='utf-8'))


if __name__ == '__main__':
  unittest.main()
