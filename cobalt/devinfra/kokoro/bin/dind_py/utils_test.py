#!/usr/bin/env python3
#
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""
Test the utils methods.
"""

import utils as sut

import unittest
from unittest import mock
import os
import subprocess


@unittest.skipIf(os.name == 'nt', 'Does not work on Windows')
class UtilsTest(unittest.TestCase):

  def setUp(self):
    return

  def tearDown(self):
    return

  @mock.patch.dict(
      os.environ, {
          'KOKORO_GIT_COMMIT_src': '1a2b3c4',
          'REGISTRY_PATH': 'us-central1-docker.pkg.dev',
          'REGISTRY_IMAGE_NAME': 'docker-build-failure',
          'PLATFORM': 'linux-x64x11',
          'KOKORO_BUILD_NUMBER': '999',
          'WORKSPACE_COBALT': '/tmp'
      },
      clear=True)
  def test_get_args_from_env(self):
    args = sut.get_args_from_env()
    self.assertEqual(args.git_sha, '1a2b3c4')
    self.assertEqual(args.registry_path, 'us-central1-docker.pkg.dev')
    self.assertEqual(args.registry_img, 'docker-build-failure')
    self.assertEqual(args.platform, 'linux-x64x11')
    self.assertEqual(args.build_num, '999')
    self.assertEqual(args.src_root, '/tmp')

  @mock.patch('subprocess.check_output')
  def test_exec_cmd(self, mock_subprocess):
    # successful call
    mock_subprocess.return_value = 0
    sut.exec_cmd('ls')  # pylint: disable=protected-access
    self.assertEqual(mock_subprocess.call_count, 1)
    # exception call
    mock_subprocess.side_effect = subprocess.CalledProcessError(0, '')
    with self.assertRaises(subprocess.CalledProcessError):
      sut.exec_cmd('ls')  # pylint: disable=protected-access
