#!/usr/bin/env python3
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Tests downloading individual and directory resources from GCS."""
import logging
import os
import tempfile
import unittest

from tools import download_from_gcs

_BUCKET = 'chromium-clang-format'
_HASH_FILE_EXT = '.sha1'
_TEST_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'testing/download_from_gcs')
_TEST_DIRECTORY = 'test_dir'
_TEST_FILE = 'clang-format.sha1'

logging_format = '[%(levelname)s:%(filename)s:%(lineno)s] %(message)s'
logging.basicConfig(
    level=logging.INFO, format=logging_format, datefmt='%H:%M:%S')


def _Sha1sMatch(file_to_read, file_to_hash):
  if not os.path.exists(file_to_read):
    logging.error('%s does not exist.', file_to_read)
    return False

  if not os.path.exists(file_to_hash):
    logging.error('%s does not exist.', file_to_hash)
    return False

  with open(file_to_read) as f:
    sha1 = f.read().strip()

  return download_from_gcs.ExtractSha1(file_to_hash) == sha1


class TestFileDownload(unittest.TestCase):
  """Download a single file from GCS and verify it."""

  def setUp(self):
    self.test_file = os.path.join(_TEST_PATH, _TEST_FILE)
    self.output_directory = tempfile.TemporaryDirectory()  # pylint:disable=consider-using-with
    self.output_file = os.path.join(self.output_directory.name, 'output')
    self.bucket = _BUCKET

  def tearDown(self):
    os.remove(self.output_file)
    self.output_directory.cleanup()

  def download_file(self, test_file, output_file):
    return download_from_gcs.MaybeDownloadFileFromGcs(
        self.bucket, sha1_file=test_file, output_file=output_file)

  def testDownloadSingleFile(self):
    self.assertTrue(self.download_file(self.test_file, self.output_file))
    self.assertTrue(_Sha1sMatch(self.test_file, self.output_file))


class DirectoryDownloadTest(unittest.TestCase):
  """Download files from GCS using a directory of sha files."""

  def setUp(self):
    self.test_directory = os.path.join(_TEST_PATH, _TEST_DIRECTORY)
    self.output_directory = tempfile.TemporaryDirectory()  # pylint:disable=consider-using-with
    self.bucket = _BUCKET

  def tearDown(self):
    self.output_directory.cleanup()

  def download_files(self, test_directory, output_directory):
    return download_from_gcs.MaybeDownloadDirectoryFromGcs(
        self.bucket,
        sha1_directory=test_directory,
        output_directory=output_directory)

  def testDownloadMultipleFiles(self):
    self.assertTrue(
        self.download_files(self.test_directory, self.output_directory.name))

    sha1_files = list(os.listdir(self.test_directory))
    output_files = list(os.listdir(self.output_directory.name))
    self.assertEqual(len(sha1_files), len(output_files))

    for output_file in output_files:
      sha1_file = output_file + _HASH_FILE_EXT
      self.assertIn(sha1_file, sha1_files)

      sha1_file = os.path.join(self.test_directory, sha1_file)
      output_file = os.path.join(self.output_directory.name, output_file)
      self.assertTrue(_Sha1sMatch(sha1_file, output_file))
