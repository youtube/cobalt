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
"""Unit tests for convert_json_to_junit_xml."""

import json
import os
import pathlib
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from convert_json_to_junit_xml import convert
from cobalt.tools.junit_mini_parser import find_failing_tests


class TestConvertJsonToJunitXml(unittest.TestCase):
  """Unit tests for convert_json_to_junit_xml."""

  def _convert(self, data):
    with tempfile.TemporaryDirectory() as tmp:
      json_path = pathlib.Path(tmp) / 'in.json'
      xml_path = pathlib.Path(tmp) / 'out.xml'
      json_path.write_text(json.dumps(data), encoding='utf-8')
      convert(str(json_path), str(xml_path))
      return ET.parse(xml_path).getroot(), find_failing_tests([str(xml_path)])

  def test_convert_vega_and_wpt_formats(self):
    root, failing = self._convert({
        'tests': [
            {
                'test_title': 'browse_smoke_test',
                'class_name': 'KabukiBrowse',
                'result': 'PASSED',
                'start_time': 1000,
                'end_time': 5000,
                'output': ['Test passed successfully'],
            },
            {
                'test_title': 'login_smoke_test',
                'test_suite': 'UnpluggedLogin',
                'test_category': 'ignored_category',
                'result': 'FAILED',
                'start_time': 6000,
                'end_time': 9500,
                'output': [
                    'Login button not found',
                    'Page DOM: <div class=\'test\'>&special;</div>',
                ],
                'errors': ['AssertionError: element not found'],
            },
            {
                'test_title': 'category_fallback_test',
                'test_category': 'infra_smoke',
                'result': 'SKIP',
            },
        ]
    })
    self.assertEqual(root.attrib['tests'], '3')
    self.assertEqual(root.attrib['failures'], '1')
    fail_text = root.find('.//failure').text
    self.assertIn('Login button not found', fail_text)
    self.assertIn('<div class=\'test\'>&special;</div>', fail_text)
    self.assertIsNotNone(
        root.find(".//testcase[@classname='infra_smoke']/skipped"))
    self.assertEqual(
        [f['name'] for f in list(failing.values())[0]],
        ['UnpluggedLogin.login_smoke_test'],
    )

  def test_convert_chromium_format(self):
    root, failing = self._convert({
        'per_iteration_data': [{
            'MyClass#TestMethod[0]': [{
                'status': 'FAILURE',
                'elapsed_time_ms': 1234,
                'output_snippet': 'Failure in TestMethod: <xml> & data',
            }],
            'NoClassTest': [{
                'status': 'CRASH',
                'elapsed_time_ms': None,
                'output_snippet': 'Crashed',
            }],
        }]
    })
    self.assertEqual(root.attrib['tests'], '2')
    self.assertEqual(root.attrib['failures'], '1')
    self.assertEqual(root.attrib['errors'], '1')
    self.assertEqual(
        root.find(".//testcase[@name='NoClassTest']").attrib['time'], '0.000')
    self.assertEqual(len(list(failing.values())[0]), 2)

  def test_convert_sanitizes_xml_characters_and_attributes(self):
    root, _ = self._convert({
        'tests': [{
            'test_title': 'test_\x00_title_\ud800',
            'class_name': 'ControlChar_\x1b_Class',
            'result': 'FAIL',
            'errors': [
                'Error with \x00 null, \x08 backspace, \x1b escape, '
                'keep \t and \n, and ]]> tag'
            ],
        }]
    })
    case_el = root.find('.//testcase')
    self.assertNotIn('\x00', case_el.attrib['name'])
    self.assertNotIn('\ud800', case_el.attrib['name'])
    self.assertNotIn('\x1b', case_el.attrib['classname'])
    fail_text = root.find('.//failure').text
    self.assertNotIn('\x00', fail_text)
    self.assertNotIn('\x08', fail_text)
    self.assertNotIn('\x1b', fail_text)
    self.assertIn('\t', fail_text)
    self.assertIn('\n', fail_text)
    self.assertIn(']]> tag', fail_text)

  def test_unknown_or_aborted_status_fails_closed(self):
    root, failing = self._convert({
        'tests': [
            {
                'test_title': 'aborted_test',
                'result': 'ABORTED',
            },
            {
                'test_title': 'null_status_test',
                'result': None,
            },
        ]
    })
    self.assertEqual(root.attrib['tests'], '2')
    self.assertEqual(root.attrib['errors'], '2')
    self.assertEqual(len(list(failing.values())[0]), 2)


if __name__ == '__main__':
  unittest.main()
