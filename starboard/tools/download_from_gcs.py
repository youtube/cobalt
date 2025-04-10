#!/usr/bin/env python
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
"""Downloads files from Google Cloud Storage given corresponding sha1s."""

import argparse
import hashlib
import logging
import os
import shutil
import stat
import tempfile
import time
import urllib.request

_BASE_GCS_URL = 'https://storage.googleapis.com'
_BUFFER_SIZE = 2 * 1024 * 1024


class GcsDownloadException(Exception):
  pass


def AddExecutableBits(filename):
  st = os.stat(filename)
  os.chmod(filename, st.st_mode | stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)


def ExtractSha1(filename):
  with open(filename, 'rb') as f:
    sha1 = hashlib.sha1()
    buf = f.read(_BUFFER_SIZE)
    while buf:
      sha1.update(buf)
      buf = f.read(_BUFFER_SIZE)
  return sha1.hexdigest()


def _DownloadFromGcsAndCheckSha1(bucket, sha1):
  url = f'{_BASE_GCS_URL}/{bucket}/{sha1}'
  with urllib.request.urlopen(url) as res:
    with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
      shutil.copyfileobj(res, tmp_file)

  if ExtractSha1(tmp_file.name) != sha1:
    raise GcsDownloadException(
        'Local and remote sha1s do not match. Skipping download.')

  return tmp_file


def MaybeDownloadFileFromGcs(bucket, sha1_file, output_file, force=False):
  """Download file from GCS if it doesn't already exist.

  Args:
    bucket: The GCS bucket that the file is stored in.
    sha1_file: The file containing the sha1 of the file to download
    output_file: The file to output to
    force: If true, will overwrite an existing output_file
  Returns:
    True if it writes a file, False otherwise.
  """
  if not os.path.exists(sha1_file):
    logging.error("Provided sha1 file %s doesn't exist", sha1_file)
  with open(sha1_file, encoding='utf-8') as fd:
    sha1 = fd.read().strip()

  if not force and os.path.exists(output_file):
    with open(output_file, 'rb') as f:
      if hashlib.sha1(f.read()).hexdigest() == sha1:
        logging.debug('%s exists and sha1s match, skipping download',
                      output_file)
        return False

  try:
    tmp_file = _DownloadFromGcsAndCheckSha1(bucket, sha1)
  except (urllib.error.URLError, GcsDownloadException) as e:
    logging.error('Download failed: \'%s\', retrying', str(e))
    time.sleep(1)
    tmp_file = _DownloadFromGcsAndCheckSha1(bucket, sha1)

  output_dir = os.path.dirname(output_file)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)

  shutil.move(tmp_file.name, output_file)
  AddExecutableBits(output_file)
  return True


def MaybeDownloadDirectoryFromGcs(bucket,
                                  sha1_directory,
                                  output_directory,
                                  force=False):
  res = True
  for filename in os.listdir(sha1_directory):
    filebase, ext = os.path.splitext(filename)
    if ext == '.sha1':
      filepath = os.path.join(sha1_directory, filename)
      output_filepath = os.path.join(output_directory, filebase)
      if not MaybeDownloadFileFromGcs(bucket, filepath, output_filepath, force):
        res = False
  return res


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-b',
      '--bucket',
      required=True,
      help='GCS bucket the resource is located in.')
  parser.add_argument(
      '-s',
      '--sha1',
      required=True,
      help='Path to file or directory containing sha1 checksum(s).')
  parser.add_argument(
      '-o',
      '--output',
      required=True,
      help='Path to file or directory to download file(s) to.')
  parser.add_argument(
      '-f',
      '--force',
      action='store_true',
      help='Replace an existing resource.')
  parser.add_argument(
      '--stamp_file',
      help='Path to stamp file to create upon script completion.')
  args = parser.parse_args()

  if os.path.isdir(args.sha1):
    MaybeDownloadDirectoryFromGcs(args.bucket, args.sha1, args.output,
                                  args.force)
  else:
    MaybeDownloadFileFromGcs(args.bucket, args.sha1, args.output, args.force)

  if args.stamp_file:
    with open(args.stamp_file, 'w', encoding='utf-8'):
      pass


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.INFO,
      format='[%(levelname)s:%(filename)s:%(lineno)s] %(message)s',
      datefmt='%H:%M:%S')
  main()
