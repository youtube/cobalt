#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Tests the archive_test_artifacts module."""

import os
import tempfile
import unittest
from unittest import mock

from cobalt.build import archive_test_artifacts


class ArchiveTestArtifactsTest(unittest.TestCase):

  def setUp(self):
    # pylint: disable=consider-using-with
    self.test_dir = tempfile.TemporaryDirectory()
    self.source_dir = os.path.join(self.test_dir.name, 'src')
    self.out_dir = os.path.join(self.source_dir, 'out', 'platform')
    self.destination_dir = os.path.join(self.test_dir.name, 'artifacts')
    os.makedirs(self.out_dir)
    os.makedirs(self.destination_dir)

  def tearDown(self):
    self.test_dir.cleanup()

  def test_create_archive_browsertests_delegation(self):
    # Setup mock collect_test_artifacts.py
    collect_script_dir = os.path.join(self.source_dir, 'cobalt', 'testing',
                                      'browser_tests', 'tools')
    os.makedirs(collect_script_dir)
    collect_script = os.path.join(collect_script_dir,
                                  'collect_test_artifacts.py')
    with open(collect_script, 'w', encoding='utf-8') as f:
      f.write('# mock script')

    with mock.patch('subprocess.check_call') as mock_call:
      archive_test_artifacts.create_archive(
          targets=['cobalt/testing/browser_tests:cobalt_browsertests'],
          source_dir=self.source_dir,
          out_dir=self.out_dir,
          destination_dir=self.destination_dir,
          archive_per_target=True,
          use_android_deps_path=False,
          compression='gz',
          compression_level=1,
          flatten_deps=True)

      expected_output_path = os.path.join(self.destination_dir,
                                          'cobalt_browsertests_deps.tar.gz')
      mock_call.assert_called_once_with([
          'vpython3', collect_script, self.out_dir, '-o', expected_output_path
      ])

  def test_create_archive_normal_target(self):
    target_name = 'base_unittests'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('base_unittests\n')

    with mock.patch(
        'cobalt.build.archive_test_artifacts._make_tar') as mock_tar:
      archive_test_artifacts.create_archive(
          targets=[f'base:{target_name}'],
          source_dir=self.source_dir,
          out_dir=self.out_dir,
          destination_dir=self.destination_dir,
          archive_per_target=True,
          use_android_deps_path=False,
          compression='gz',
          compression_level=1,
          flatten_deps=True)

      mock_tar.assert_called_once()
      call_args = mock_tar.call_args[0]
      # archive_path
      self.assertEqual(
          call_args[0],
          os.path.join(self.destination_dir, f'{target_name}_deps.tar.gz'))
      # file_lists
      file_lists = call_args[3]
      self.assertIn('base_unittests', file_lists[0][0])

  def test_create_archive_browsertest_explicit_path(self):
    # Setup mock collect_test_artifacts.py
    collect_script_dir = os.path.join(self.source_dir, 'cobalt', 'testing',
                                      'browser_tests', 'tools')
    os.makedirs(collect_script_dir)
    collect_script = os.path.join(collect_script_dir,
                                  'collect_test_artifacts.py')
    with open(collect_script, 'w', encoding='utf-8') as f:
      f.write('# mock script')

    explicit_path = os.path.join(self.test_dir.name, 'custom.tar.gz')

    with mock.patch('subprocess.check_call') as mock_call:
      archive_test_artifacts.create_archive(
          targets=['cobalt/testing/browser_tests:cobalt_browsertests'],
          source_dir=self.source_dir,
          out_dir=self.out_dir,
          destination_dir=self.destination_dir,
          archive_per_target=True,
          use_android_deps_path=False,
          compression='gz',
          compression_level=1,
          flatten_deps=True,
          browsertest_archive_path=explicit_path)

      mock_call.assert_called_once_with(
          ['vpython3', collect_script, self.out_dir, '-o', explicit_path])


if __name__ == '__main__':
  unittest.main()
