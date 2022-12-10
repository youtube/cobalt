#!/usr/bin/python
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
"""Copy the out directory and filter out files and folders we don't need.

This script filters out large files, e.g. ".mp4" and ".deb" files, and
intermediate build files, that are not critical to the ability to launch Cobalt.
The purpose of applying this filter is to reduce the size of the destination out
folder.

Usage:
  copy_and_filter_out_dir.py -s <source_out_dir> -d <dest_out_dir>

source_out_dir is the absolute path to the source folder from which the out
directory is copied.
e.g. '/usr/local/google/home/{username}/cobalt/src/out/linux-x64x11_qa
dest_out_dir the absolute path to the destination folder into which the out
directory is copied.
e.g. '/tmp/out/linux-x64x11_qa'

"""

import argparse
import logging
import os
import shutil
import sys


def _FilterFile(file_name):
  name, extension = os.path.splitext(file_name)
  return 'ninja' in file_name or name in [
      'bytecode_builtins_list_generator', 'gyp-win-tool', 'mksnapshot',
      'protoc', 'torque', 'v8_build_config'
  ] or extension in ['.mp4', '.deb', '.d', '.o', '.stamp', '.tmp', '.rsp']


def _FilterDir(dir_name):
  filter_dir = ['gen', 'gypfiles', 'obj', 'obj.host', 'gradle', 'pyproto']
  return dir_name in filter_dir


def _IterateAndFilter(source_path, dest_path, copymap):
  """Iterate the source_path and filter out files and folders we don't need.

  Add the source to destination file pairs to the copy map.

  Args:
    source_path: the absolute path to the source folder to be copied from.
    dest_path: the absolute path to the destination folder to be copied to.
    copymap: the map of file pairs from source to destination.
  """

  # Iterate and filter files
  files = next(os.walk(source_path))[2]
  for f in files:
    if not _FilterFile(f):
      source_file_path = os.path.join(source_path, f)
      dest_file_path = os.path.join(dest_path, f)
      copymap[source_file_path] = dest_file_path

  # Iterate and filter directories
  dirs = next(os.walk(source_path))[1]
  for d in dirs:
    if not _FilterDir(d):
      source_dir_path = os.path.join(source_path, d)
      dest_dir_path = os.path.join(dest_path, d)
      _IterateAndFilter(source_dir_path, dest_dir_path, copymap)


def CopyAndFilterOutDir(source_out_dir, dest_out_dir):
  """Copy the out directory and filter out files and folders.

  Args:
    source_out_dir: the absolute path to the source folder from which the out
      directory is copied.
    dest_out_dir: the absolute path to the destination folder into which the out
      directory is copied.
  Returns:
    0 on success.
  """
  source_out_dir = os.path.abspath(source_out_dir)
  dest_out_dir = os.path.abspath(dest_out_dir)
  logging.info('Copying and filtering %s to %s', source_out_dir, dest_out_dir)

  copies = {}

  if not os.path.exists(source_out_dir):
    logging.error('Skipping CopyAndFilterOutDir, %s does not exist',
                  source_out_dir)
    return 0

  _IterateAndFilter(source_out_dir, dest_out_dir, copies)
  for source, dest in copies.items():
    dirname = os.path.dirname(dest)
    if not os.path.exists(dirname):
      os.makedirs(dirname)
    shutil.copy(source, dest)
    logging.info('%s -> %s', source, dest)

  logging.info('Done.')
  return 0


if __name__ == '__main__':
  logging_format = '%(asctime)s %(levelname)-8s %(message)s'
  logging_level = logging.INFO
  logging.basicConfig(
      level=logging_level, format=logging_format, datefmt='%m-%d %H:%M')

  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-s',
      '--source_out_dir',
      required=True,
      help='The absolute path to the source folder from which'
      'the out directory is copied.')
  parser.add_argument(
      '-d',
      '--dest_out_dir',
      required=True,
      help='The absolute path to the destination folder into which'
      'the out directory is copied.')
  args = parser.parse_args()

  sys.exit(CopyAndFilterOutDir(args.source_out_dir, args.dest_out_dir))
