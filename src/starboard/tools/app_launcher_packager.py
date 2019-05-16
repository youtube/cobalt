#!/usr/bin/python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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


################################################################################
#                                   API                                        #
################################################################################


def CopyAppLauncherTools(repo_root, dest_root, additional_sub_dirs=[]):
  """Copies app launcher related files to the destination root.
  repo_root: The 'src' path that will be used for packaging.
  dest_root: The directory where the src files will be stored.
  additional_sub_dirs: Some platforms may need to include certain dependencies
    beyond the default include directories. Each element in this list will be
    a path with assumed root at src/. For example ['third_party/X'] would
    include path 'src/third_party/X/**/*.py' in the packaging operation. For a
    list of default paths see _PYTHON_SRC_DIRS in this file."""
  _CopyAppLauncherTools(repo_root, dest_root, additional_sub_dirs)


def MakeZipArchive(src, output_zip):
  """Convenience function to zip up all files in the src directory (produced
  as dest_root argument in CopyAppLauncherTools()) which will create a zip
  file with the relative root being the src directory."""
  _MakeZipArchive(src, output_zip)


################################################################################
#                                  IMPL                                        #
################################################################################


import argparse
import logging
import os
import shutil
import sys

import _env  # pylint: disable=unused-import
from paths import REPOSITORY_ROOT
from paths import THIRD_PARTY_ROOT
sys.path.append(THIRD_PARTY_ROOT)
import starboard.tools.platform
import jinja2


# Default python directories to find code.
_PYTHON_SRC_DIRS = [
    'starboard',
    'cobalt',                # TODO: Test and possibly prune.
    'buildbot',              # Needed because of device_server.
    'lbshell',               # TODO: Test and possibly prune.
    'third_party/jinja2',    # Required by this app_launcher_packager.py script.
    'third_party/markupsafe' # Required by third_party/jinja2
]


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


def _FindPythonAndCertFiles(source_root):
  logging.info('Searching in ' + source_root + ' for python files.')
  file_list = []
  for root, _, files in os.walk(source_root):
    # Eliminate any locally built files under the out directory.
    if _IsOutDir(source_root, root):
      continue
    for f in files:
      if f.endswith('.py') or f.endswith('.crt') or f.endswith('.key'):
        file_list.append(os.path.join(root, f))
  return file_list


def _WritePlatformsInfo(repo_root, dest_root):
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


def _CopyAppLauncherTools(repo_root, dest_root, additional_sub_dirs):
  # Step 1: Remove previous output directory if it exists
  if os.path.isdir(dest_root):
    shutil.rmtree(dest_root)
  # Step 2: Find all python files from specified search directories.
  subdirs = _PYTHON_SRC_DIRS + additional_sub_dirs
  copy_list = []
  for d in subdirs:
    flist = _FindPythonAndCertFiles(os.path.join(repo_root, d))
    copy_list.extend(flist)
  # Copy all src/*.py from repo_root without recursing down.
  for f in os.listdir(repo_root):
    src = os.path.join(repo_root, f)
    if os.path.isfile(src) and src.endswith('.py'):
      copy_list.append(src)
  # Order by file path string and remove any duplicate paths.
  copy_list = list(set(copy_list))
  copy_list.sort()
  # Step 3: Copy the src files to the destination directory.
  for src in copy_list:
    tail_path = os.path.relpath(src, repo_root)
    dst = os.path.join(dest_root, tail_path)
    d = os.path.dirname(dst)
    if not os.path.isdir(d):
      os.makedirs(d)
    logging.info(src + ' -> ' + dst)
    shutil.copy2(src, dst)
  # Step 4: Re-write the platform infos file in the new repo copy.
  _WritePlatformsInfo(repo_root, dest_root)


def _MakeZipArchive(src, output_zip):
  if os.path.isfile(output_zip):
    os.unlink(output_zip)
  logging.info('Creating a zip file of the app launcher package')
  logging.info(src + ' -> ' + output_zip)
  tmp_file = shutil.make_archive(src, 'zip', src)
  shutil.move(tmp_file, output_zip)


def main(command_args):
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-d',
      '--destination_root',
      required=True,
      help='The path to the root of the destination folder into which the '
      'python scripts are packaged.')
  args = parser.parse_args(command_args)
  CopyAppLauncherTools(REPOSITORY_ROOT, args.destination_root)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
