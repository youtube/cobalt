#!/usr/bin/env python
#
# Copyright 2017 Google Inc. All Rights Reserved.
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
#
"""Tests the command_line module."""

import os
import unittest

import _env  # pylint: disable=unused-import
from starboard.tools import command_line
import starboard.tools.config
import starboard.tools.platform


_A_CONFIG = starboard.tools.config.GetAll()[0]
_A_PLATFORM = starboard.tools.platform.GetAll()[0]


def _RestoreMapping(target, source):
  target.clear()
  for key, value in source.iteritems():
    target[key] = value


def _ClearEnviron():
  del os.environ['BUILD_CONFIGURATION']
  del os.environ['BUILD_TYPE']
  del os.environ['BUILD_PLATFORM']


def _SetEnvironConfig(config):
  os.environ['BUILD_TYPE'] = config


def _SetEnvironPlatform(platform):
  os.environ['BUILD_PLATFORM'] = platform


def _SetEnvironBuildConfiguration(config, platform):
  os.environ['BUILD_CONFIGURATION'] = '%s_%s' % (platform, config)


def _SetEnviron(config, platform):
  _SetEnvironConfig(config)
  _SetEnvironPlatform(platform)
  _SetEnvironBuildConfiguration(config, platform)


class CommandLineTest(unittest.TestCase):

  def setUp(self):
    self.environ = os.environ.copy()
    _ClearEnviron()

  def tearDown(self):
    _RestoreMapping(os.environ, self.environ)

  def testNoEnvironmentRainyDayNoArgs(self):
    parser = command_line.CreateParser()
    with self.assertRaises(SystemExit):
      parser.parse_args([])

  def testNoEnvironmentRainyDayNoConfig(self):
    parser = command_line.CreateParser()
    with self.assertRaises(SystemExit):
      parser.parse_args(['-p', _A_PLATFORM])

  def testNoEnvironmentRainyDayNoPlatform(self):
    parser = command_line.CreateParser()
    with self.assertRaises(SystemExit):
      parser.parse_args(['-c', _A_CONFIG])

  def testNoEnvironmentSunnyDay(self):
    parser = command_line.CreateParser()
    args = parser.parse_args(['-c', _A_CONFIG, '-p', _A_PLATFORM])
    self.assertEqual(_A_CONFIG, args.config)
    self.assertEqual(_A_PLATFORM, args.platform)

  def testDefaultsSunnyDay(self):
    _SetEnviron(_A_CONFIG, _A_PLATFORM)
    parser = command_line.CreateParser()
    args = parser.parse_args([])
    self.assertEqual(_A_CONFIG, args.config)
    self.assertEqual(_A_PLATFORM, args.platform)

  def testDefaultsRainyDayBadConfig(self):
    _SetEnviron('badconfig', _A_PLATFORM)
    parser = command_line.CreateParser()
    with self.assertRaises(SystemExit):
      parser.parse_args([])

  def testDefaultsRainyDayBadPlatform(self):
    _SetEnviron(_A_CONFIG, 'badplatform')
    parser = command_line.CreateParser()
    with self.assertRaises(SystemExit):
      parser.parse_args([])

  def testBadEnvironmentSunnyDay(self):
    _SetEnviron('badconfig', 'badplatform')
    parser = command_line.CreateParser()
    args = parser.parse_args(['-c', _A_CONFIG, '-p', _A_PLATFORM])
    self.assertEqual(_A_CONFIG, args.config)
    self.assertEqual(_A_PLATFORM, args.platform)

  def testInconsistentEnvironmentSunnyDay(self):
    _SetEnvironBuildConfiguration(_A_CONFIG, _A_PLATFORM)
    parser = command_line.CreateParser()
    args = parser.parse_args([])
    self.assertEqual(_A_CONFIG, args.config)
    self.assertEqual(_A_PLATFORM, args.platform)


if __name__ == '__main__':
  unittest.main()
