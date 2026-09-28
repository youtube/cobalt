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
"""Unit tests for convert_json_to_junit_xml.py."""

import json
import pathlib
import tempfile
import unittest
import xml.etree.ElementTree as ET

from convert_json_to_junit_xml import convert
from cobalt.tools.junit_mini_parser import find_failing_tests


class TestConvertJsonToJunitXml(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.dir_path = pathlib.Path(self.temp_dir.name)

  def tearDown(self):
    self.temp_dir.cleanup()

  def test_convert_vega_json_with_cdata(self):
    vega_data = {
        "result": {"tests_executed": 2, "tests_failed": 1},
        "tests": [
            {
                "test_title": "browse_smoke_test",
                "class_name": "KabukiBrowse",
                "result": "PASSED",
                "start_time": 1000,
                "end_time": 5000,
                "output": ["Test passed successfully"]
            },
            {
                "test_title": "login_smoke_test",
                "class_name": "UnpluggedLogin",
                "result": "FAILED",
                "start_time": 6000,
                "end_time": 9500,
                "output": ["Login button not found", "Page DOM: <div class='test'>&special;</div>"],
                "errors": ["AssertionError: element not found"]
            }
        ]
    }
    json_path = self.dir_path / "vega_report.json"
    xml_path = self.dir_path / "vega_output.xml"
    json_path.write_text(json.dumps(vega_data), encoding="utf-8")

    convert(str(json_path), str(xml_path))
    self.assertTrue(xml_path.is_file())

    # Verify JUnit structure
    tree = ET.parse(xml_path)
    root = tree.getroot()
    self.assertEqual(root.attrib["tests"], "2")
    self.assertEqual(root.attrib["failures"], "1")

    # Verify failure element and CDATA contents
    fail_el = root.find(".//failure")
    self.assertIsNotNone(fail_el)
    self.assertIn("Login button not found", fail_el.text)
    self.assertIn("<div class='test'>&special;</div>", fail_el.text)

    # Verify junit_mini_parser can parse it
    failing = find_failing_tests([str(xml_path)])
    self.assertEqual(len(failing), 1)
    failures_list = list(failing.values())[0]
    self.assertEqual(len(failures_list), 1)
    self.assertEqual(failures_list[0]["name"], "UnpluggedLogin.login_smoke_test")

  def test_convert_chromium_format(self):
    chromium_data = {
        "per_iteration_data": [
            {
                "MyClass#TestMethod": [
                    {
                        "status": "FAILURE",
                        "elapsed_time_ms": 1234,
                        "output_snippet": "Failure in TestMethod: <xml> & data"
                    }
                ]
            }
        ]
    }
    json_path = self.dir_path / "chromium_report.json"
    xml_path = self.dir_path / "chromium_output.xml"
    json_path.write_text(json.dumps(chromium_data), encoding="utf-8")

    convert(str(json_path), str(xml_path))
    self.assertTrue(xml_path.is_file())

    failing = find_failing_tests([str(xml_path)])
    self.assertEqual(len(failing), 1)
    failures_list = list(failing.values())[0]
    self.assertEqual(failures_list[0]["name"], "MyClass.TestMethod")

  def test_convert_with_embedded_cdata_close_tag(self):
    data = {
        "tests": [
            {
                "test_title": "test_cdata_escape",
                "class_name": "EscapeTest",
                "result": "FAIL",
                "errors": ["Assertion failure with embedded ]]> tag"],
            }
        ]
    }
    json_path = self.dir_path / "cdata_report.json"
    xml_path = self.dir_path / "cdata_output.xml"
    json_path.write_text(json.dumps(data), encoding="utf-8")

    convert(str(json_path), str(xml_path))
    self.assertTrue(xml_path.is_file())

    tree = ET.parse(xml_path)
    root = tree.getroot()
    fail_el = root.find(".//failure")
    self.assertIsNotNone(fail_el)
    self.assertIn("embedded ]]> tag", fail_el.text)


if __name__ == "__main__":
  unittest.main()
