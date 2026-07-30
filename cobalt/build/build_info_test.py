#!/usr/bin/env python3
#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Tests the build_info module."""

import os
import subprocess
import tempfile
import unittest

from cobalt.build import build_info

_TEST_BUILD_NUMBER = 1234 + build_info.COMMIT_COUNT_BUILD_ID_OFFSET


# TODO(b/282040638): fix and re-enabled this
@unittest.skipIf(os.name == 'nt', 'Broken on Windows')
class GetBuildIdTest(unittest.TestCase):

  def setUp(self):
    self.test_dir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.original_cwd = os.getcwd()
    os.chdir(self.test_dir.name)
    subprocess.check_call(['git', 'init'])
    subprocess.check_call(['git', 'config', 'user.name', 'pytest'])
    subprocess.check_call(['git', 'config', 'user.email', 'pytest@pytest.com'])

  def tearDown(self):
    os.chdir(self.original_cwd)
    self.test_dir.cleanup()

  def make_commit(self, message='Temporary commit'):
    with tempfile.NamedTemporaryFile('w', dir=self.test_dir.name) as temp_file:
      subprocess.check_call(['git', 'add', temp_file.name])
    subprocess.check_call(['git', 'commit', '-m', message])

  def make_commit_with_build_number(self, build_number=_TEST_BUILD_NUMBER):
    message = f'Subject line\n\nBUILD_NUMBER={build_number}'
    self.make_commit(message)

  def testSanity(self):
    self.make_commit()
    head_rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'])
    self.assertNotEqual(head_rev.strip().decode('utf-8'), '')

  def testGetBuildNumberFromCommitsSunnyDay(self):
    self.make_commit_with_build_number()
    build_number, _ = build_info.get_build_id_and_git_rev_from_commits(
        cwd=self.test_dir.name)
    self.assertEqual(int(build_number), _TEST_BUILD_NUMBER)

  def testGetBuildNumberFromCommitsSunnyDayGetMostRecent(self):
    num_commits = 5
    for i in range(num_commits):
      self.make_commit_with_build_number(
          build_info.COMMIT_COUNT_BUILD_ID_OFFSET + i)
    build_number, _ = build_info.get_build_id_and_git_rev_from_commits(
        cwd=self.test_dir.name)
    self.assertEqual(
        int(build_number),
        num_commits + build_info.COMMIT_COUNT_BUILD_ID_OFFSET - 1)

  def testGetBuildNumberFromCommitsRainyDayInvalidBuildNumber(self):
    self.make_commit()
    self.make_commit(f'BUILD_NUMBER={_TEST_BUILD_NUMBER}')
    build_number, _ = build_info.get_build_id_and_git_rev_from_commits(
        cwd=self.test_dir.name)
    self.assertIsNone(build_number)

  def testGetBuildNumberFromCommitCountSunnyDay(self):
    num_commits = 5
    for _ in range(num_commits):
      self.make_commit()
    build_number = build_info.get_build_id_from_commit_count(
        cwd=self.test_dir.name)
    self.assertEqual(
        int(build_number),
        num_commits + build_info.COMMIT_COUNT_BUILD_ID_OFFSET)

  def testCommitsOutrankCommitCount(self):
    self.make_commit()
    self.make_commit_with_build_number()
    self.make_commit()
    build_number = build_info.get_build_id(cwd=self.test_dir.name)
    self.assertEqual(int(build_number), _TEST_BUILD_NUMBER)

  def testFallbackToCommitCount(self):
    num_commits = 5
    for _ in range(num_commits):
      self.make_commit()
    build_number = build_info.get_build_id(cwd=self.test_dir.name)
    self.assertEqual(
        int(build_number),
        num_commits + build_info.COMMIT_COUNT_BUILD_ID_OFFSET)


if __name__ == '__main__':
  unittest.main()
