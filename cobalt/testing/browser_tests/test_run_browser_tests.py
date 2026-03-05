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
"""Tests for the Cobalt browser test runner script."""

import argparse
import importlib.util
import os
import shutil
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest import mock

# Load run_browser_tests.py as a module
script_path = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'run_browser_tests.py')
spec = importlib.util.spec_from_file_location('run_browser_tests', script_path)
run_browser_tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run_browser_tests)


class TestCobaltTestRunner(unittest.TestCase):
  """Unit tests for the CobaltTestRunner class."""

  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.binary_path = os.path.join(self.test_dir, 'fake_binary')
    with open(self.binary_path, 'w', encoding='utf-8') as f:
      f.write('#!/bin/sh\nexit 0')
    os.chmod(self.binary_path, 0o755)

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def _create_runner(self, **kwargs):
    args = argparse.Namespace(
        binary=self.binary_path,
        gtest_filter=None,
        gtest_output=None,
        gtest_total_shards=0,
        gtest_shard_index=0,
        tl_total_shards=0,
        tl_shard_index=0,
        user_data_dir=None,
        json_results_file=None,
        logcat_output_file=None)
    for k, v in kwargs.items():
      setattr(args, k, v)
    return run_browser_tests.CobaltTestRunner(args, [])

  def test_is_valid_test_name(self):
    """Verifies that the script correctly identifies valid gtest names."""
    runner = self._create_runner()
    self.assertTrue(runner.is_valid_test_name('MySuite'))
    self.assertTrue(runner.is_valid_test_name('Test_Case_1'))
    self.assertFalse(runner.is_valid_test_name('Total: 14, P: 14'))
    self.assertFalse(runner.is_valid_test_name('Invalid Name!'))
    self.assertFalse(runner.is_valid_test_name(''))

  def test_parse_and_sort_tests(self):
    """Verifies parsing of --gtest_list_tests output and sorting."""
    runner = self._create_runner()
    gtest_output = ('SuiteA.\n'
                    '  Test1\n'
                    '  Test2\n'
                    '  PRE_Test2\n'
                    '  PRE_PRE_Test2\n'
                    'SuiteB.\n'
                    '  Test1\n'
                    '  DISABLED_Test2\n')
    tests = runner.parse_and_sort_tests(gtest_output)
    expected = [
        'SuiteA.Test1',
        'SuiteA.PRE_PRE_Test2',
        'SuiteA.PRE_Test2',
        'SuiteA.Test2',
        'SuiteB.Test1',
    ]
    self.assertEqual(tests, expected)

  def test_sharding_logic(self):
    """Verifies sharding logic."""
    tests = [f'Test.{i}' for i in range(10)]

    # Shard 0 of 2
    runner0 = self._create_runner(gtest_total_shards=2, gtest_shard_index=0)
    self.assertEqual(
        runner0.filter_tests_for_shard(tests),
        ['Test.0', 'Test.2', 'Test.4', 'Test.6', 'Test.8'])

    # Shard 1 of 2
    runner1 = self._create_runner(gtest_total_shards=2, gtest_shard_index=1)
    self.assertEqual(
        runner1.filter_tests_for_shard(tests),
        ['Test.1', 'Test.3', 'Test.5', 'Test.7', 'Test.9'])

  def test_xml_merging(self):
    """Verifies merging of individual test XML results."""
    results_xml = os.path.join(self.test_dir, 'results.xml')
    runner = self._create_runner(gtest_output=f'xml:{results_xml}')
    runner._initialize_xml()  # pylint: disable=protected-access

    # Create a fake individual XML result
    temp_xml = os.path.join(self.test_dir, 'temp.xml')
    with open(temp_xml, 'w', encoding='utf-8') as f:
      f.write(
          '<testsuites><testsuite name="SuiteA" tests="1">'
          '<testcase name="Test1" classname="SuiteA"/></testsuite></testsuites>'
      )

    runner._merge_test_xml('SuiteA.Test1', temp_xml)  # pylint: disable=protected-access
    runner._finalize_and_write_xml(1, 0)  # pylint: disable=protected-access

    self.assertTrue(os.path.exists(results_xml))
    tree = ET.parse(results_xml)
    root = tree.getroot()
    self.assertEqual(root.attrib['tests'], '1')
    self.assertEqual(root.attrib['failures'], '0')
    self.assertEqual(len(root.findall('testsuite')), 1)
    self.assertEqual(root.find('testsuite').attrib['name'], 'SuiteA')

  def test_json_results_file_fallback(self):
    """Verifies that --json-results-file is used if --gtest_output is missing.
    """
    results_xml = os.path.join(self.test_dir, 'results.xml')
    # Use json_results_file instead of gtest_output
    runner = self._create_runner(
        gtest_output=None, json_results_file=results_xml)
    self.assertEqual(runner.xml_output_file, results_xml)

    runner._initialize_xml()  # pylint: disable=protected-access
    temp_xml = os.path.join(self.test_dir, 'temp.xml')
    with open(temp_xml, 'w', encoding='utf-8') as f:
      f.write(
          '<testsuites><testsuite name="SuiteA" tests="1">'
          '<testcase name="Test1" classname="SuiteA"/></testsuite></testsuites>'
      )

    runner._merge_test_xml('SuiteA.Test1', temp_xml)  # pylint: disable=protected-access
    runner._finalize_and_write_xml(1, 0)  # pylint: disable=protected-access

    self.assertTrue(os.path.exists(results_xml))

  def test_xml_crash_handling(self):
    """Verifies that a crash result is synthesized when XML is missing."""
    results_xml = os.path.join(self.test_dir, 'results.xml')
    runner = self._create_runner(gtest_output=f'xml:{results_xml}')
    runner._initialize_xml()  # pylint: disable=protected-access

    # Pass None as temp_xml_path to simulate crash/missing file
    runner._merge_test_xml('SuiteA.TestCrash', None)  # pylint: disable=protected-access
    runner._finalize_and_write_xml(1, 1)  # pylint: disable=protected-access

    tree = ET.parse(results_xml)
    root = tree.getroot()
    self.assertEqual(root.attrib['failures'], '1')
    suite = root.find('testsuite')
    self.assertEqual(suite.attrib['name'], 'SuiteA')
    testcase = suite.find('testcase')
    self.assertEqual(testcase.attrib['name'], 'TestCrash')
    self.assertIsNotNone(testcase.find('failure'))

  @mock.patch('subprocess.call', return_value=0)
  def test_selective_flags(self, mock_call):
    """Verifies that Linux-specific flags are only added for real binaries."""
    # 1. Real binary should have extra flags
    real_binary = os.path.join(self.test_dir, 'cobalt_browsertests')
    with open(real_binary, 'w', encoding='utf-8') as f:
      f.write('#!/bin/sh\nexit 0')
    os.chmod(real_binary, 0o755)

    runner = self._create_runner(binary=real_binary)
    runner._run_single_test('Suite.Test', 0)  # pylint: disable=protected-access
    call_args = mock_call.call_args[0][0]
    self.assertIn('--no-sandbox', call_args)
    self.assertIn('--ozone-platform=starboard', call_args)

    # 2. Wrapper script (runner) should NOT have extra flags
    mock_call.reset_mock()
    runner_script = os.path.join(self.test_dir, 'bin/run_cobalt_browsertests')
    os.makedirs(os.path.dirname(runner_script), exist_ok=True)
    with open(runner_script, 'w', encoding='utf-8') as f:
      f.write('#!/bin/sh\nexit 0')
    os.chmod(runner_script, 0o755)

    runner2 = self._create_runner(binary=runner_script)
    runner2._run_single_test('Suite.Test', 0)  # pylint: disable=protected-access
    call_args2 = mock_call.call_args[0][0]
    self.assertNotIn('--no-sandbox', call_args2)
    self.assertNotIn('--ozone-platform=starboard', call_args2)

  @mock.patch('subprocess.check_output')
  @mock.patch('subprocess.call')
  def test_full_run(self, mock_call, mock_check_output):
    """Verifies the full run() method execution."""
    results_xml = os.path.join(self.test_dir, 'results.xml')
    runner = self._create_runner(gtest_output=f'xml:{results_xml}')

    # Mock test listing
    mock_check_output.return_value = 'Suite.\n  Test1\n  Test2\n'

    # Mock test execution
    # First test succeeds, second fails
    def mock_run_side_effect(cmd, **_unused_kwargs):
      # Extract temp XML path from cmd
      xml_path = None
      for arg in cmd:
        if arg.startswith('--gtest_output=xml:'):
          xml_path = arg[19:]
          break

      if xml_path:
        with open(xml_path, 'w', encoding='utf-8') as f:
          if 'Test1' in cmd[1]:  # gtest_filter
            f.write('<testsuites><testsuite name="Suite" tests="1">'
                    '<testcase name="Test1"/></testsuite></testsuites>')
            return 0
          else:
            f.write(
                '<testsuites><testsuite name="Suite" tests="1" failures="1">'
                '<testcase name="Test2"><failure message="fail"/></testcase>'
                '</testsuite></testsuites>')
            return 1
      return 0

    mock_call.side_effect = mock_run_side_effect

    failed_count = runner.run()
    self.assertEqual(failed_count, 1)

    # Verify final XML
    tree = ET.parse(results_xml)
    root = tree.getroot()
    self.assertEqual(root.attrib['tests'], '2')
    self.assertEqual(root.attrib['failures'], '1')
    self.assertEqual(len(root.findall('testsuite')), 2)


if __name__ == '__main__':
  unittest.main()
