#!/usr/bin/env python3
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest
import unittest.mock

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import ninjalog_uploader


class NinjalogUploaderTest(unittest.TestCase):
  def test_IsGoogler(self):
    with unittest.mock.patch('subprocess.run') as run_mock:
      run_mock.return_value.returncode = 0
      run_mock.return_value.stdout = ('Login as foo@google.com\n'
                                      'goma is ready to use')
      self.assertTrue(ninjalog_uploader.IsGoogler())

    with unittest.mock.patch('subprocess.run') as run_mock:
      run_mock.return_value.returncode = 1
      self.assertFalse(ninjalog_uploader.IsGoogler())

    with unittest.mock.patch('subprocess.run') as run_mock:
      run_mock.return_value.returncode = 0
      run_mock.return_value.stdout = ''
      self.assertFalse(ninjalog_uploader.IsGoogler())

    with unittest.mock.patch('subprocess.run') as run_mock:
      run_mock.return_value.returncode = 0
      run_mock.return_value.stdout = 'Login as foo@example.com\n'
      self.assertFalse(ninjalog_uploader.IsGoogler())

  def test_parse_gn_args(self):
    self.assertEqual(ninjalog_uploader.ParseGNArgs(json.dumps([])), {})

    # Extract current configs from GN's output json.
    self.assertEqual(ninjalog_uploader.ParseGNArgs(json.dumps([
        {
            'current': {'value': 'true'},
            'default': {'value': 'false'},
            'name': 'is_component_build'
        },
        {
            'default': {'value': '"x64"'},
            'name': 'host_cpu'
        },
    ])), {
        'is_component_build': 'true',
        'host_cpu': '"x64"',
    })

    self.assertEqual(ninjalog_uploader.ParseGNArgs(json.dumps([
        {
            'current': {'value': 'true'},
            'default': {'value': 'false'},
            'name': 'is_component_build'
        },
        {
            'current': {'value': 'false'},
            'default': {'value': 'false'},
            'name': 'use_goma'
        },
    ])), {'is_component_build': 'true',
          'use_goma': 'false'})

  def test_get_ninjalog(self):
    # No args => default to cwd.
    self.assertEqual(ninjalog_uploader.GetNinjalog(['ninja']),
                     './.ninja_log')

    # Specified by -C case.
    self.assertEqual(
        ninjalog_uploader.GetNinjalog(['ninja', '-C', 'out/Release']),
        'out/Release/.ninja_log')
    self.assertEqual(
        ninjalog_uploader.GetNinjalog(['ninja', '-Cout/Release']),
        'out/Release/.ninja_log')

    # Invalid -C flag case.
    self.assertEqual(ninjalog_uploader.GetNinjalog(['ninja', '-C']),
                     './.ninja_log')

    # Multiple target directories => use the last directory.
    self.assertEqual(ninjalog_uploader.GetNinjalog(
        ['ninja', '-C', 'out/Release', '-C', 'out/Debug']),
        'out/Debug/.ninja_log')

  def test_get_build_target_from_command_line(self):
    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(
            ['python3', 'ninja.py', 'chrome']), ['chrome'])

    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(['python3',
                                                         'ninja.py']), [])

    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(
            ['python3', 'ninja.py', '-j', '1000', 'chrome']), ['chrome'])

    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(
            ['python3', 'ninja.py', 'chrome', '-j', '1000']), ['chrome'])

    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(
            ['python3', 'ninja.py', '-C', 'chrome']), [])

    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(
            ['python3', 'ninja.py', '-Cout/Release', 'chrome']), ['chrome'])

    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine(
            ['python3', 'ninja.py', '-C', 'out/Release', 'chrome', 'all']),
        ['chrome', 'all'])

  @unittest.skipIf(sys.platform == 'win32', 'posix path test')
  def test_get_build_target_from_command_line_filter_posix(self):
    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine([
            'python3', 'ninja.py', '-C', 'out/Release', 'chrome', 'all',
            '/path/to/foo', '-p'
        ]), ['chrome', 'all'])

  @unittest.skipUnless(sys.platform == 'win32', 'Windows path test')
  def test_get_build_target_from_command_line_filter_win(self):
    self.assertEqual(
        ninjalog_uploader.GetBuildTargetFromCommandLine([
            'python3', 'ninja.py', '-C', 'out/Release', 'chrome', 'all',
            'C:\\path\\to\\foo', '-p'
        ]), ['chrome', 'all'])

  def test_get_j_flag(self):
    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja']), None)

    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja','-j', '1000']), 1000)

    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja','-j', '1000a']), None)

    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja','-j', 'a']), None)

    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja','-j1000']), 1000)

    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja','-ja']), None)

    self.assertEqual(ninjalog_uploader.GetJflag(
        ['ninja','-j']), None)


if __name__ == '__main__':
  unittest.main()
