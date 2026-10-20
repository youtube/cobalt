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
"""Tests for generate_crash_report."""

import pathlib
import subprocess
import sys
import tempfile
import unittest

from generate_crash_report import _extract_crash, write_junit_xml


class GenerateCrashReportTest(unittest.TestCase):

  def test_extract_crash_standard_gtest_format(self):
    log_content = ('[==========] Running 1 test from 1 test suite.\n'
                   '[----------] Global test environment set-up.\n'
                   '[----------] 1 test from MyTestSuite\n'
                   '[ RUN      ] MyTestSuite.MyTest\n'
                   'Some log output before crash...\n')
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
      f.write(log_content)
      temp_path = pathlib.Path(f.name)

    try:
      crash = _extract_crash(temp_path)
      self.assertIsNotNone(crash)
      suite, name, log = crash
      self.assertEqual(suite, 'MyTestSuite')
      self.assertEqual(name, 'MyTest')
      self.assertIn('[ RUN      ] MyTestSuite.MyTest\n', log)
    finally:
      temp_path.unlink(missing_ok=True)

  def test_extract_crash_with_android_runner_prefix(self):
    # pylint: disable=line-too-long
    log_content = (
        'I 22:23:20.750 1215.904s _RunTestsOnDevice(44081HFAG01B1Q)  [----------] 1 test from SubresourceLoadingTest\n'
        'I 22:23:20.800 1215.954s _RunTestsOnDevice(44081HFAG01B1Q)  [ RUN      ] SubresourceLoadingTest.URLLoaderFactoryInInitialEmptyDoc_NewFrameWithoutSrc\n'
        'I 22:23:21.000 1216.154s _RunTestsOnDevice(44081HFAG01B1Q)  Crashing now...\n'
    )
    # pylint: enable=line-too-long
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
      f.write(log_content)
      temp_path = pathlib.Path(f.name)

    try:
      crash = _extract_crash(temp_path)
      self.assertIsNotNone(crash)
      suite, name, log = crash
      self.assertEqual(suite, 'SubresourceLoadingTest')
      self.assertEqual(name,
                       'URLLoaderFactoryInInitialEmptyDoc_NewFrameWithoutSrc')
      self.assertIn('Crashing now...', log)
    finally:
      temp_path.unlink(missing_ok=True)

  def test_extract_crash_passed_test_not_reported(self):
    log_content = ('[ RUN      ] MyTestSuite.MyTest\n'
                   '[       OK ] MyTestSuite.MyTest (10 ms)\n')
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
      f.write(log_content)
      temp_path = pathlib.Path(f.name)

    try:
      crash = _extract_crash(temp_path)
      self.assertIsNone(crash)
    finally:
      temp_path.unlink(missing_ok=True)

  def test_extract_crash_failed_test_not_reported_as_crash(self):
    log_content = ('[ RUN      ] MyTestSuite.MyTest\n'
                   'Failure details...\n'
                   '[  FAILED  ] MyTestSuite.MyTest (10 ms)\n')
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
      f.write(log_content)
      temp_path = pathlib.Path(f.name)

    try:
      crash = _extract_crash(temp_path)
      self.assertIsNone(crash)
    finally:
      temp_path.unlink(missing_ok=True)

  def test_write_junit_xml(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      xml_path = pathlib.Path(temp_dir) / 'test_output.xml'
      write_junit_xml(xml_path, 'MySuite', 'MyTest', 'Some error log')
      content = xml_path.read_text(encoding='utf-8')
      self.assertIn('<testsuite name="MySuite"', content)
      self.assertIn('<testcase name="MyTest" classname="MySuite"', content)
      self.assertIn('<![CDATA[ Some error log ]]>', content)

  def test_main_with_fallback_suite_when_no_gtest_crash_detected(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      log_path = pathlib.Path(temp_dir) / 'log.txt'
      xml_path = pathlib.Path(temp_dir) / 'out.xml'
      log_path.write_text(
          'Some arbitrary runner crash log without gtest markers\n')
      script_path = pathlib.Path(__file__).parent / 'generate_crash_report.py'
      res = subprocess.run(
          [
              sys.executable,
              str(script_path),
              str(log_path),
              str(xml_path), '--fallback-suite', 'my_target_suite'
          ],
          capture_output=True,
          text=True,
          check=False,
      )
      self.assertEqual(res.returncode, 0)
      self.assertTrue(xml_path.is_file())
      content = xml_path.read_text(encoding='utf-8')
      self.assertIn('<testsuite name="my_target_suite"', content)
      self.assertIn(
          '<testcase name="my_target_suite" classname="my_target_suite"',
          content)
      crash_marker = xml_path.with_suffix('.crash')
      self.assertTrue(crash_marker.is_file())
      self.assertEqual(
          crash_marker.read_text(encoding='utf-8'),
          'my_target_suite.my_target_suite\n')


if __name__ == '__main__':
  unittest.main()
