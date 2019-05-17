#!/usr/bin/env python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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


"""Runs a cobalt archive without any dependencies on external code."""


import sys
import unittest


import _env  # pylint: disable=relative-import,unused-import
import cobalt.tools.cobalt_archive_runner as car_runner


class CobaltArchiveRunnerArgTest(unittest.TestCase):

  def testArchiveOnly(self):
    input_args = ['ARCHIVE.ZIP']
    args = car_runner._ParseArgs(input_args)
    self.assertEqual('ARCHIVE.ZIP', args.archive)
    self.assertEqual(None, args.temp_dir)
    self.assertEqual(False, args.keep)
    self.assertEqual(None, args.command)
    self.assertEqual([], args.command_arg)

  def testTrampoline(self):
    input_args = ['ARCHIVE.ZIP', '--', 'run_tests', '-t', 'nplb']
    args = car_runner._ParseArgs(input_args)
    self.assertEqual('ARCHIVE.ZIP', args.archive)
    self.assertEqual('run_tests', args.command)
    self.assertEqual(None, args.temp_dir)
    self.assertEqual(False, args.keep)
    self.assertEqual('run_tests', args.command)
    self.assertEqual(['-t', 'nplb'], args.command_arg)

  def testKeep(self):
    input_args = ['--keep', 'ARCHIVE.ZIP']
    args = car_runner._ParseArgs(input_args)
    self.assertEqual('ARCHIVE.ZIP', args.archive)
    self.assertEqual(None, args.command)
    self.assertEqual(None, args.temp_dir)
    self.assertEqual(True, args.keep)
    self.assertEqual(None, args.command)
    self.assertEqual([], args.command_arg)

  def testBasicKeepSetWhenTempDirIsUsed(self):
    input_args = [
        '--temp_dir', 'temp/path', 'ARCHIVE.ZIP', '--',
        'run_tests', '-t', 'nplb'
    ]
    args = car_runner._ParseArgs(input_args)
    self.assertEqual('ARCHIVE.ZIP', args.archive)
    self.assertEqual('run_tests', args.command)
    self.assertEqual('temp/path', args.temp_dir)
    self.assertEqual(True, args.keep)
    self.assertEqual('run_tests', args.command)
    self.assertEqual(['-t', 'nplb'], args.command_arg)


if __name__ == '__main__':
  unittest.main()
  sys.exit(0)
