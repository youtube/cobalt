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
Test the DinD common methods.
"""

import dind_common as sut

import unittest
from unittest import mock
import os
import subprocess

TEST_DIR_PATH = os.path.dirname(os.path.realpath(__file__))

FAILURE_IMAGE = ('us-central1-docker.pkg.dev/cobalt-kokoro-gar/qa/' +
                 'docker-build-failure:latest')
TEST_FAILURE_IMAGE = ('us-central1-docker.pkg.dev/cobalt-kokoro-gar/qa/' +
                      'docker-build-failure:test')
BROKEN_IMAGE = 'us-central1-docker.pkg.dev/fake-image:fake-tag'

COMPOSE_FILE_PATH = f'{TEST_DIR_PATH}/test-docker-compose.yaml'


@unittest.skipIf(os.name == 'nt', 'Does not work on Windows')
class DindCommonTest(unittest.TestCase):

  def setUp(self):
    return

  def tearDown(self):
    return

  def test_pull_tag_push_image(self):
    sut.pull_image(FAILURE_IMAGE)
    sut.tag_image(FAILURE_IMAGE, TEST_FAILURE_IMAGE)
    sut.push_image(TEST_FAILURE_IMAGE)

  def test_pull_image_fails(self):
    with self.assertRaises(subprocess.CalledProcessError):
      sut.pull_image(BROKEN_IMAGE)

  def test_get_local_image_name(self):
    self.assertEqual('my_image',
                     sut.get_local_image_name('my_service', COMPOSE_FILE_PATH))

  @mock.patch('dind_common.get_local_image_name')
  @mock.patch.dict('dind_common._PLATFORM_TO_SERVICE_MAP',
                   {'foo_test': 'foo_test'})
  def test_run_docker_build(self, mock_get_local_image_name):
    mock_get_local_image_name.return_value = 'foo_test'
    sut.run_docker_build(
        'foo_test', 'foo_test:baz_tag', compose_file=COMPOSE_FILE_PATH)
