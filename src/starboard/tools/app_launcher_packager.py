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

import argparse
import fnmatch
import logging
import os
import shutil
import string
import sys
import tempfile

import _env  # pylint: disable=unused-import
from paths import REPOSITORY_ROOT
from paths import THIRD_PARTY_ROOT

sys.path.append(THIRD_PARTY_ROOT)
# pylint: disable=g-import-not-at-top,g-bad-import-order,wrong-import-position
from starboard.tools import command_line
from starboard.tools import log_level
from starboard.tools import port_symlink
import starboard.tools.platform

# Default python directories to app launcher resources.
_INCLUDE_FILE_PATTERNS = [('cobalt', '*.py'), ('starboard', '*.py'),
                          ('starboard', '*.pfx'),
                          ('starboard/tools', 'platform.py.template')]

_INCLUDE_BLACK_BOX_TESTS_PATTERNS = [
    # Black box and web platform tests have non-py assets, so everything
    # is picked up.
    ('cobalt/black_box_tests', '*'),
    ('third_party/web_platform_tests', '*'),
    ('third_party/proxy_py', '*'),
]

# Do not allow .git directories to make it into the build.
_EXCLUDE_DIRECTORY_PATTERNS = ['.git']


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

  Returns:
    true if d in in source_root/out.
  """
  out_dir = os.path.join(source_root, 'out')
  return out_dir in d


def _FindFilesRecursive(  # pylint: disable=missing-docstring
    src_root, glob_pattern):
  src_root = os.path.normpath(src_root)
  logging.info('Searching in %s for %s type files.', src_root, glob_pattern)
  file_list = []
  for root, dirs, files in os.walk(src_root, topdown=True):
    # Prunes when using os.walk with topdown=True
    for d in list(dirs):
      if d in _EXCLUDE_DIRECTORY_PATTERNS:
        dirs.remove(d)
    # Eliminate any locally built files under the out directory.
    if _IsOutDir(src_root, root):
      continue
    files = fnmatch.filter(files, glob_pattern)
    for f in files:
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
  dest_dir = os.path.join(dest_root, 'starboard', 'tools')
  platforms_map = {}
  for p in starboard.tools.platform.GetAll():
    platform_path = os.path.relpath(
        starboard.tools.platform.Get(p).path, repo_root)
    # Store posix paths even on Windows so MH Linux hosts can use them.
    # The template has code to re-normalize them when used on Windows hosts.
    platforms_map[p] = platform_path.replace('\\', '/')
  with open(os.path.join(current_dir, 'platform.py.template')) as c:
    template = string.Template(c.read())
    with open(os.path.join(dest_dir, 'platform.py'), 'w+') as f:
      sub = template.substitute(platforms_map=platforms_map)
      f.write(sub.encode('utf-8'))
  logging.info('Finished baking in platform info files.')


def CopyAppLauncherTools(repo_root,
                         dest_root,
                         additional_glob_patterns=None,
                         include_black_box_tests=True):
  """Copies app launcher related files to the destination root.

  Args:
    repo_root: The 'src' path that will be used for packaging.
    dest_root: The directory where the src files will be stored.
    additional_glob_patterns: Some platforms may need to include certain
      dependencies beyond the default include file patterns. The results here
      will be merged in with _INCLUDE_FILE_PATTERNS.
    include_black_box_tests: If True then the resources for the black box tests
      are included.
  """
  dest_root = _PrepareDestination(dest_root)
  copy_list = _GetSourceFilesList(repo_root, additional_glob_patterns,
                                  include_black_box_tests)
  _CopyFiles(repo_root, dest_root, copy_list)


def _PrepareDestination(dest_root):  # pylint: disable=missing-docstring
  # Make sure dest_root is an absolute path.
  logging.info('Copying App Launcher tools to = %s', dest_root)
  dest_root = os.path.normpath(dest_root)
  if not os.path.isabs(dest_root):
    dest_root = os.path.join(os.getcwd(), dest_root)
  dest_root = port_symlink.ToLongPath(dest_root)
  logging.info('Absolute destination path = %s', dest_root)
  # Remove previous output directory if it exists
  if os.path.isdir(dest_root):
    shutil.rmtree(dest_root)
  return dest_root


def _GetSourceFilesList(  # pylint: disable=missing-docstring
    repo_root,
    additional_glob_patterns=None,
    include_black_box_tests=True):
  # Find all glob files from specified search directories.
  include_glob_patterns = _INCLUDE_FILE_PATTERNS
  if additional_glob_patterns:
    include_glob_patterns += additional_glob_patterns
  if include_black_box_tests:
    include_glob_patterns += _INCLUDE_BLACK_BOX_TESTS_PATTERNS
  copy_list = []
  for d, glob_pattern in include_glob_patterns:
    flist = _FindFilesRecursive(os.path.join(repo_root, d), glob_pattern)
    copy_list.extend(flist)
  # Copy all src/*.py from repo_root without recursing down.
  for f in os.listdir(repo_root):
    src = os.path.join(repo_root, f)
    if os.path.isfile(src) and src.endswith('.py'):
      copy_list.append(src)
  # Order by file path string and remove any duplicate paths.
  copy_list = list(set(copy_list))
  copy_list.sort()
  return copy_list


def _CopyFiles(  # pylint: disable=missing-docstring
    repo_root, dest_root, copy_list):
  # Copy the src files to the destination directory.
  folders_logged = set()
  for src in copy_list:
    tail_path = os.path.relpath(src, repo_root)
    dst = os.path.join(dest_root, tail_path)
    d = os.path.dirname(dst)
    if not os.path.isdir(d):
      os.makedirs(d)
    src_folder = os.path.dirname(src)
    if src_folder not in folders_logged:
      folders_logged.add(src_folder)
      logging.info(src_folder + ' -> ' + os.path.dirname(dst))
    shutil.copy2(src, dst)
  # Re-write the platform infos file in the new repo copy.
  _WritePlatformsInfo(repo_root, dest_root)


def MakeZipArchive(src, output_zip):
  """Convenience function to zip up all files in the src directory.

  Intended for use with the dest_root output from CopyAppLauncherTools() to
  create a zip file with the relative root being the src directory.

  Args:
    src: Path to the directory of files to zip up.
    output_zip: Path to the zip file to create.
  """
  if os.path.isfile(output_zip):
    os.unlink(output_zip)
  logging.info('Creating a zip file of the app launcher package')
  logging.info(src + ' -> ' + output_zip)
  tmp_file = shutil.make_archive(src, 'zip', src)
  shutil.move(tmp_file, output_zip)


def main(command_args):
  parser = argparse.ArgumentParser()
  command_line.AddLoggingArguments(parser, default='warning')
  dest_group = parser.add_mutually_exclusive_group(required=True)
  dest_group.add_argument(
      '-d',
      '--destination_root',
      help='The path to the root of the destination folder into which the '
      'application resources are packaged.')
  dest_group.add_argument(
      '-z',
      '--zip_file',
      help='The path to a zip file into which the application resources are '
      'packaged.')
  dest_group.add_argument(
      '-l',
      '--list',
      action='store_true',
      help='List to stdout the application resources relative to the current '
      'directory.')
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Enables verbose logging. For more control over the '
      "logging level use '--log_level' instead.")
  args = parser.parse_args(command_args)

  log_level.InitializeLogging(args)

  if args.destination_root:
    CopyAppLauncherTools(REPOSITORY_ROOT, args.destination_root)
  elif args.zip_file:
    try:
      temp_dir = tempfile.mkdtemp(prefix='cobalt_app_launcher_')
      CopyAppLauncherTools(REPOSITORY_ROOT, temp_dir)
      MakeZipArchive(temp_dir, args.zip_file)
    finally:
      shutil.rmtree(temp_dir)
  elif args.list:
    src_files = []
    for src_file in _GetSourceFilesList(REPOSITORY_ROOT):
      # Skip paths with '$' since they won't get through the Ninja generator.
      if '$' in src_file:
        continue
      # Relative to CWD where gyp ran this; same as '<(DEPTH)' in gyp file.
      src_file = os.path.relpath(src_file)
      # Forward slashes for gyp, even on Windows.
      src_file = src_file.replace('\\', '/')
      src_files.append(src_file)
    out = ' '.join(src_files)
    return out.strip()
  return 0


def DoMain(argv):
  """Script main function."""
  return main(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
