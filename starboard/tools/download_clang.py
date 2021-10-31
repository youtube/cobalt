#!/usr/bin/env python3
#
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Tool to update Clang toolchain used by Starboard, from Chromium repository"""

import argparse
import os
import shutil
import sys
import tarfile
import tempfile
import time

try:
  import requests
except ImportError:
  # Fail with an error at call time.
  requests = None


def DownloadUrl(url, output_file):
  chunk_size = 4096
  print_n_dots = 40

  response = requests.get(url, stream=True, allow_redirects=True)
  response.raise_for_status()
  content_length = int(response.headers.get('content-length', 0))
  if not content_length:
    output_file.write(response.content)
  else:
    downloaded = 0
    dots_printed = 0
    for data in response.iter_content(chunk_size=chunk_size):
      output_file.write(data)
      downloaded += len(data)
      dots = print_n_dots * downloaded // content_length
      sys.stdout.write('.' * (dots - dots_printed))
      sys.stdout.flush()
      dots_printed = dots
  print(' Done.')


def Retry(function, exceptions_tuple, max_tries=3, retry_wait_s=5):
  for i in range(max_tries):
    try:
      function()
      break
    except exceptions_tuple as e:
      print(e)
      if i == max_tries - 1:
        raise
      print('Retrying in %d s ...' % retry_wait_s)
      sys.stdout.flush()
      time.sleep(retry_wait_s)
      continue


def DownloadAndUnpack(url, output_dir):
  with tempfile.TemporaryFile() as tmpfile:
    Retry(lambda: DownloadUrl(url, tmpfile), (requests.RequestException,))
    tmpfile.seek(0)

    if not os.path.exists(output_dir):
      os.makedirs(output_dir)

    t = tarfile.open(mode='r:gz', fileobj=tmpfile)
    t.extractall(path=output_dir)


def UpdateClang(target_dir, revision):
  """ Downloads, unpacks and stamps a particular prebuilt Clang version
  from Chromium pre-tested binary releases repo.
  """
  stamp_file = os.path.join(target_dir, 'cr_build_revision')

  try:
    with open(stamp_file, 'r') as f:
      if f.read().rstrip() == revision:
        return
  except IOError:
    pass

  if os.path.exists(target_dir):
    shutil.rmtree(target_dir)

  cds_file = 'clang-%s.tgz' % revision
  cds_full_url = os.environ.get(
      'CDS_CLANG_BUCKET_OVERRIDE',
      'https://commondatastorage.googleapis.com/chromium-browser-clang'
  ) + '/Linux_x64/' + cds_file
  try:
    DownloadAndUnpack(cds_full_url, target_dir)
  except requests.RequestException as e:
    print(e)
    print('Failed to download prebuilt clang %s' % cds_full_url)
    raise

  with open(stamp_file, 'w') as f:
    f.write(revision)
    f.write('\n')


def main():
  parser = argparse.ArgumentParser(description='Update clang.')
  parser.add_argument('dest', help='Where to extract the clang package.')
  parser.add_argument('revision', help='Force the given clang revision.')
  args = parser.parse_args()

  return UpdateClang(os.path.abspath(args.dest), args.revision)


if __name__ == '__main__':
  sys.exit(main())
