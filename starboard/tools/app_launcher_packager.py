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
import os
import platform
import shutil
import sys
import threading

from paths import REPOSITORY_ROOT
from paths import THIRD_PARTY_ROOT
sys.path.append(THIRD_PARTY_ROOT)
import jinja2


def _MakeDir(d):
  """Make the specified directory and any parents in the path.

  Args:
    d: Directory name to create.
  """
  if d and not os.path.isdir(d):
    root = os.path.dirname(d)
    _MakeDir(root)
    os.mkdir(d)


class FileCopier(threading.Thread):
  """Threaded class to copy a file."""

  def __init__(self, source, dest):
    threading.Thread.__init__(self)
    self.source = source
    self.dest = dest

  def run(self):
    shutil.copy2(self.source, self.dest)


def CopyPythonFiles(source_root, dest_root):
  """Copy all python files to the destination folder.

  Copy from source to destination while maintaining the directory structure.

  Args:
    source_root: Absolute path to the root of files to be copied.
    dest_root: Destination into which files will be copied.
  """
  copies = {}
  for root, _, files in os.walk(source_root):
    for f in files:
      if f.endswith('.py'):
        source_file = os.path.join(root, f)
        dest_file = source_file.replace(source_root, dest_root)
        copies[source_file] = dest_file

  file_copiers = []
  for source, dest in copies.iteritems():
    _MakeDir(os.path.dirname(dest))
    copier = FileCopier(source, dest)
    file_copiers.append(copier)
    copier.start()

  for copier in file_copiers:
    copier.join()


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
  current_file = os.path.abspath(__file__)
  current_dir = os.path.dirname(current_file)
  dest_dir = current_dir.replace(repo_root, dest_root)

  platforms_map = {}
  for p in platform.GetAll():
    platform_path = platform.Get(p).path.replace(repo_root, dest_root)
    platforms_map[p] = platform_path

  template = jinja2.Template(
      open(os.path.join(current_dir, 'platform.py.template')).read())
  with open(os.path.join(dest_dir, 'platform.py'), 'w+') as f:
    template.stream(platforms_map=platforms_map).dump(f, encoding='utf-8')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-d',
      '--destination_root',
      required=True,
      help='The path to the root of the destination folder into which the'
      'python scripts are packaged.')
  args = parser.parse_args()

  dest_root = args.destination_root

  CopyPythonFiles(REPOSITORY_ROOT, dest_root)
  WritePlatformsInfo(REPOSITORY_ROOT, dest_root)
  return 0


if __name__ == '__main__':
  sys.exit(main())
