#!/usr/bin/python
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
"""Extracts python files and writes platforms information.

This script extracts the app launcher scripts (and all their dependencies) from
the Cobalt source tree, and then packages them into a user-specified location so
that the app launcher can be run independent of the Cobalt source tree.
"""

import argparse
import logging
import os
import shutil
import sys

import _env  # pylint: disable=unused-import
from paths import REPOSITORY_ROOT
from paths import THIRD_PARTY_ROOT
sys.path.append(THIRD_PARTY_ROOT)
import jinja2
import starboard.tools.platform


def _MakeDir(d):
  """Make the specified directory and any parents in the path.

  Args:
    d: Directory name to create.
  """
  if d and not os.path.isdir(d):
    root = os.path.dirname(d)
    _MakeDir(root)
    os.mkdir(d)


def _IsOutDir(source_root, d):
  """Check if d is under source_root/out directory.

  Args:
    source_root: Absolute path to the root of the files to be copied.
    d: Directory to be checked.
  """
  out_dir = os.path.join(source_root, 'out')
  return out_dir in d


def CopyPythonFiles(source_root, dest_root):
  """Copy all python files to the destination folder.

  Copy from source to destination while maintaining the directory structure.

  Args:
    source_root: Absolute path to the root of files to be copied.
    dest_root: Destination into which files will be copied.
  """
  logging.info('Searching in ' + source_root + ' for python files.')
  file_list = []
  for root, _, files in os.walk(source_root):
    # Eliminate any locally built files under the out directory.
    if _IsOutDir(source_root, root):
      continue
    for f in files:
      if f.endswith('.py'):
        source_file = os.path.join(root, f)
        dest_file = source_file.replace(source_root, dest_root)
        file_list.append((source_file, dest_file))

  logging.info('Starting copy of ' + str(len(file_list)) + ' python files.')
  for (source, dest) in file_list:
    _MakeDir(os.path.dirname(dest))
    shutil.copy2(source, dest)
  logging.info('Copy of python files finished.')


def WritePlatformsInfo(repo_root, dest_root):
  """Get platforms' information and write the platform.py based on a template.

  Platform.py is responsible for enumerating all supported platforms in the
  Cobalt source tree.  Since we are extracting the app launcher script from the
  Cobalt source tree, this function records which platforms are supported while
  the Cobalt source tree is still available and bakes them in to a newly created
  platform.py file that does not depend on the Cobalt source tree.

  Args:
    repo_root: Absolute path to the root of the repository where platforms'
      information is retrieved.
    dest_root: Absolute path to the root of the new repository into which
      platforms' information to be written.
  """
  logging.info('Baking platform info files.')
  current_file = os.path.abspath(__file__)
  current_dir = os.path.dirname(current_file)
  dest_dir = current_dir.replace(repo_root, dest_root)

  platforms_map = {}
  for p in starboard.tools.platform.GetAll():
    platform_path = os.path.relpath(
        starboard.tools.platform.Get(p).path, repo_root)
    platforms_map[p] = platform_path

  template = jinja2.Template(
      open(os.path.join(current_dir, 'platform.py.template')).read())
  with open(os.path.join(dest_dir, 'platform.py'), 'w+') as f:
    template.stream(platforms_map=platforms_map).dump(f, encoding='utf-8')
  logging.info('Finished baking in platform info files.')


def CopyAppLauncherTools(repo_root, dest_root):
  CopyPythonFiles(repo_root, dest_root)
  WritePlatformsInfo(repo_root, dest_root)

  # Create an extra zip file of the app launcher package so that users have
  # the option of downloading a single file which is much faster, especially
  # on x20.
  logging.info('Creating a zip file of the app launcher package.')
  app_launcher_zip_file = shutil.make_archive('app_launcher', 'zip', dest_root)
  shutil.move(app_launcher_zip_file, dest_root)


def main(command_args):
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-d',
      '--destination_root',
      required=True,
      help='The path to the root of the destination folder into which the'
      'python scripts are packaged.')
  args = parser.parse_args(command_args)
  CopyAppLauncherTools(REPOSITORY_ROOT, args.destination_root)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
