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
"""Unit tests for junit_xml_utils.py."""

import os
import tempfile
import unittest
import xml.etree.ElementTree as ET

import junit_xml_utils


class TestJunitXmlUtils(unittest.TestCase):
  """Tests for junit_xml_utils."""

  def test_merge_reports(self):
    with tempfile.TemporaryDirectory() as test_dir:
      base_xml = os.path.join(test_dir, 'base.xml')
      retry_xml = os.path.join(test_dir, 'retry.xml')
      output_xml = os.path.join(test_dir, 'output.xml')

      with open(base_xml, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="2" failures="1" errors="0">
  <testsuite name="Suite1" tests="2" failures="1" errors="0">
    <testcase name="Test1" classname="Suite1"/>
    <testcase name="Test2" classname="Suite1">
      <failure message="failed"/>
    </testcase>
  </testsuite>
</testsuites>
""")

      with open(retry_xml, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="0" errors="0">
  <testsuite name="Suite1" tests="1" failures="0" errors="0">
    <testcase name="Test2" classname="Suite1"/>
  </testsuite>
</testsuites>
""")

      success = junit_xml_utils.merge_reports(base_xml, retry_xml, output_xml)
      self.assertTrue(success)

      tree = ET.parse(output_xml)
      root = tree.getroot()

      suite = root.find('testsuite')
      self.assertIsNotNone(suite)
      self.assertEqual(suite.get('name'), 'Suite1')

      cases = suite.findall('testcase')
      self.assertEqual(len(cases), 2)

      test2 = None
      for case in cases:
        if case.get('name') == 'Test2':
          test2 = case
          break

      self.assertIsNotNone(test2)
      self.assertEqual(len(test2.findall('failure')), 0)

      self.assertEqual(suite.get('failures'), '0')
      self.assertEqual(root.get('failures'), '0')

  def test_filter_failures(self):
    with tempfile.TemporaryDirectory() as test_dir:
      input_xml = os.path.join(test_dir, 'input.xml')
      output_xml = os.path.join(test_dir, 'output.xml')

      with open(input_xml, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="3" failures="1" errors="1">
  <testsuite name="Suite1" tests="3" failures="1" errors="1">
    <testcase name="Test1" classname="Suite1"/>
    <testcase name="Test2" classname="Suite1">
      <failure message="failed"/>
    </testcase>
    <testcase name="Test3" classname="Suite1">
      <error message="error"/>
    </testcase>
  </testsuite>
</testsuites>
""")

      success = junit_xml_utils.filter_failures(input_xml, output_xml)
      self.assertTrue(success)

      tree = ET.parse(output_xml)
      root = tree.getroot()

      suite = root.find('testsuite')
      self.assertIsNotNone(suite)
      self.assertEqual(suite.get('tests'), '2')

      cases = suite.findall('testcase')
      self.assertEqual(len(cases), 2)
      self.assertEqual(cases[0].get('name'), 'Test2')
      self.assertEqual(cases[1].get('name'), 'Test3')

  def test_merge_reports_single_suite(self):
    with tempfile.TemporaryDirectory() as test_dir:
      base_xml = os.path.join(test_dir, 'base.xml')
      retry_xml = os.path.join(test_dir, 'retry.xml')
      output_xml = os.path.join(test_dir, 'output.xml')

      with open(base_xml, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="Suite1" tests="2" failures="1" errors="0">
  <testcase name="Test1" classname="Suite1"/>
  <testcase name="Test2" classname="Suite1">
    <failure message="failed"/>
  </testcase>
</testsuite>
""")

      with open(retry_xml, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="Suite1" tests="1" failures="0" errors="0">
  <testcase name="Test2" classname="Suite1"/>
</testsuite>
""")

      success = junit_xml_utils.merge_reports(base_xml, retry_xml, output_xml)
      self.assertTrue(success)

      tree = ET.parse(output_xml)
      root = tree.getroot()

      self.assertEqual(root.tag, 'testsuite')
      self.assertEqual(root.get('failures'), '0')

  def test_filter_failures_single_suite(self):
    with tempfile.TemporaryDirectory() as test_dir:
      input_xml = os.path.join(test_dir, 'input.xml')
      output_xml = os.path.join(test_dir, 'output.xml')

      with open(input_xml, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="Suite1" tests="2" failures="1" errors="0">
  <testcase name="Test1" classname="Suite1"/>
  <testcase name="Test2" classname="Suite1">
    <failure message="failed"/>
  </testcase>
</testsuite>
""")

      success = junit_xml_utils.filter_failures(input_xml, output_xml)
      self.assertTrue(success)

      tree = ET.parse(output_xml)
      root = tree.getroot()

      self.assertEqual(root.tag, 'testsuite')
      self.assertEqual(root.get('tests'), '1')
      cases = root.findall('testcase')
      self.assertEqual(len(cases), 1)
      self.assertEqual(cases[0].get('name'), 'Test2')


if __name__ == '__main__':
  unittest.main()
