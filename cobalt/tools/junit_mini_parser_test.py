"""Tests for the simple JUnit parser."""
#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest
from unittest.mock import mock_open, patch

from cobalt.tools.junit_mini_parser import find_failing_tests


class TestFindFailingTests(unittest.TestCase):
  """Test cases for find_failing_tests in junit_mini_parser."""

  def test_no_failing_tests(self):
    xml_content = """
    <testsuites>
      <testsuite name="TestSuite1">
        <testcase name="test_success_1"/>
        <testcase name="test_success_2"/>
      </testsuite>
    </testsuites>
    """
    with patch('builtins.open', mock_open(read_data=xml_content)):
      failing_tests = find_failing_tests(['dummy_file.xml'])
      self.assertEqual(failing_tests, {})

  def test_single_failing_test(self):
    xml_content = """
    <testsuites>
      <testsuite name="TestSuiteA">
        <testcase name="test_pass"/>
        <testcase name="test_fail">
          <failure message="Assertion failed">...</failure>
        </testcase>
      </testsuite>
    </testsuites>
    """
    with patch('builtins.open', mock_open(read_data=xml_content)):
      failing_tests = find_failing_tests(['report.xml'])
      expected = {'report.xml': ['TestSuiteA.test_fail']}
      self.assertEqual(failing_tests, expected)

  def test_multiple_failing_tests_in_single_file(self):
    xml_content = """
    <testsuites>
      <testsuite name="UnitTests">
        <testcase name="test_one_passed"/>
        <testcase name="test_two_failed">
          <failure message="Something went wrong">...</failure>
        </testcase>
        <testcase name="test_three_errored">
          <error message="An exception occurred">...</error>
        </testcase>
        <testcase name="test_four_passed"/>
      </testsuite>
    </testsuites>
    """
    with patch('builtins.open', mock_open(read_data=xml_content)):
      failing_tests = find_failing_tests(['results.xml'])
      expected = {
          'results.xml': [
              'UnitTests.test_two_failed', 'UnitTests.test_three_errored'
          ]
      }
      self.assertEqual(failing_tests, expected)

  def test_failing_tests_in_multiple_files(self):
    xml_content1 = """
    <testsuites>
      <testsuite name="ModuleA">
        <testcase name="test_alpha_ok"/>
        <testcase name="test_beta_fail">
          <failure>...</failure>
        </testcase>
      </testsuite>
    </testsuites>
    """
    xml_content2 = """
    <testsuites>
      <testsuite name="ModuleB">
        <testcase name="test_gamma_error">
          <error>...</error>
        </testcase>
        <testcase name="test_delta_ok"/>
      </testsuite>
    </testsuites>
    """
    with patch(
        'builtins.open',
        side_effect=[
            mock_open(read_data=xml_content1).return_value,
            mock_open(read_data=xml_content2).return_value
        ]):
      failing_tests = find_failing_tests(['report1.xml', 'report2.xml'])
      expected = {
          'report1.xml': ['ModuleA.test_beta_fail'],
          'report2.xml': ['ModuleB.test_gamma_error']
      }
      self.assertEqual(failing_tests, expected)

  def test_empty_testsuite_name(self):
    xml_content = """
    <testsuites>
      <testsuite>
        <testcase name="test_one_failed">
          <failure>...</failure>
        </testcase>
      </testsuite>
    </testsuites>
    """
    with patch('builtins.open', mock_open(read_data=xml_content)):
      failing_tests = find_failing_tests(['empty_name.xml'])
      expected = {'empty_name.xml': ['empty_name.xml.test_one_failed']}
      self.assertEqual(failing_tests, expected)


if __name__ == '__main__':
  unittest.main()
