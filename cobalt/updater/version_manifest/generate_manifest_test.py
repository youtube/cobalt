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
"""Tests the generate_manifest module."""

import json
import os
import tempfile
import unittest

from cobalt.updater.version_manifest import generate_manifest


class CobaltToEvergreenVersionTest(unittest.TestCase):

  def testLtsVersions(self):
    self.assertEqual(
        generate_manifest.cobalt_to_evergreen_version('27.lts.10'), '7.10.2')
    self.assertEqual(
        generate_manifest.cobalt_to_evergreen_version('27.lts.11'), '7.11.2')
    self.assertEqual(
        generate_manifest.cobalt_to_evergreen_version('27.lts.1'), '7.1.2')
    self.assertEqual(
        generate_manifest.cobalt_to_evergreen_version('25.lts.40'), '5.40.2')
    self.assertEqual(
        generate_manifest.cobalt_to_evergreen_version('28.lts.0'), '8.0.2')

  def testMinorVersionIsNotDivided(self):
    # 25.lts.30 becomes 5.30.Z, not 5.3.Z.
    self.assertEqual(
        generate_manifest.cobalt_to_evergreen_version('25.lts.30'), '5.30.2')

  def testRejectsNonLtsVersions(self):
    for version in ('27.master.0', '27.lts', 'lts.10', '27.lts.10.1',
                    '27.LTS.10', 'v27.lts.10', '27.lts.10 ', ''):
      with self.subTest(version=version):
        with self.assertRaises(ValueError):
          generate_manifest.cobalt_to_evergreen_version(version)

  def testRejectsPreEvergreenMajorVersions(self):
    with self.assertRaises(ValueError):
      generate_manifest.cobalt_to_evergreen_version('20.lts.1')


class IsLtsVersionTest(unittest.TestCase):

  def testLtsVersion(self):
    self.assertTrue(generate_manifest.is_lts_version('27.lts.10'))

  def testMasterVersion(self):
    self.assertFalse(generate_manifest.is_lts_version('27.master.0'))


class WriteManifestTest(unittest.TestCase):

  def testWritesExpectedManifest(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      output_path = os.path.join(temp_dir, 'manifest.json')
      generate_manifest.write_manifest('27.lts.10', output_path)
      with open(output_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
    self.assertEqual(
        manifest, {
            'manifest_version': 2,
            'name': 'Cobalt',
            'description': 'Cobalt',
            'version': '7.10.2',
        })


class MainTest(unittest.TestCase):

  def testSucceedsForLtsVersion(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      output_path = os.path.join(temp_dir, 'manifest.json')
      self.assertEqual(
          generate_manifest.main(
              ['--cobalt-version', '27.lts.11', '--output', output_path]), 0)
      with open(output_path, 'r', encoding='utf-8') as f:
        self.assertEqual(json.load(f)['version'], '7.11.2')

  def testFailsForMasterVersion(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      output_path = os.path.join(temp_dir, 'manifest.json')
      self.assertEqual(
          generate_manifest.main(
              ['--cobalt-version', '27.master.0', '--output', output_path]), 1)
      self.assertFalse(os.path.exists(output_path))


if __name__ == '__main__':
  unittest.main()
