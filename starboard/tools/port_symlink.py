#!/usr/bin/env python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""A portable interface for symlinking."""

import argparse
import logging
import os
import shutil
import sys

import _env  # pylint: disable=relative-import,unused-import

from starboard.tools import util


def IsWindows():
  return sys.platform in ['win32', 'cygwin']


def ToLongPath(path):
  """Converts to a path that supports long filenames."""
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    return win_symlink.ToDevicePath(path)
  else:
    return path


def IsSymLink(path):
  """Platform neutral version os os.path.islink()."""
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    return win_symlink.IsReparsePoint(path)
  else:
    return os.path.islink(path)


def MakeSymLink(target_path, link_path):
  """Makes a symlink.

  Args:
    target_path: target path to be linked to
    link_path: path to place the link

  Returns:
    None
  """
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    win_symlink.CreateReparsePoint(target_path, link_path)
  else:
    util.MakeDirs(os.path.dirname(link_path))
    os.symlink(target_path, link_path)


def ReadSymLink(link_path):
  """Returns the path (abs. or rel.) to the folder referred to by link_path."""
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    path = win_symlink.ReadReparsePoint(link_path)
  else:
    try:
      path = os.readlink(link_path)
    except OSError:
      path = None
  return path


def DelSymLink(link_path):
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    win_symlink.UnlinkReparsePoint(link_path)
  else:
    os.unlink(link_path)


def Rmtree(path):
  """See Rmtree() for documentation of this function."""
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    func = win_symlink.RmtreeShallow
  elif not os.path.islink(path):
    func = shutil.rmtree
  else:
    os.unlink(path)
    return
  if os.path.exists(path):
    func(path)


def OsWalk(root_dir, topdown=True, onerror=None, followlinks=False):
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    return win_symlink.OsWalk(root_dir, topdown, onerror, followlinks)
  else:
    return os.walk(root_dir, topdown, onerror, followlinks)


def _CreateArgumentParser():
  """Creates an argument parser for port_symlink."""

  class MyParser(argparse.ArgumentParser):

    def error(self, message):
      sys.stderr.write('error: %s\n' % message)
      self.print_help()
      sys.exit(2)

  help_msg = ('Example 1:\n'
              '  python port_link.py --link "target_path" "link_path"\n\n'
              'Example 2:\n'
              '  python port_link.py --link "../target_path" "link_path"\n\n')
  # Enables new lines in the description and epilog.
  formatter_class = argparse.RawDescriptionHelpFormatter
  parser = MyParser(epilog=help_msg, formatter_class=formatter_class)
  parser.add_argument(
      '--link',
      help='The target path and link path to be used when '
      'creating the symbolic link.',
      metavar='"path"',
      nargs=2)
  parser.add_argument(
      '-f',
      '--force',
      action='store_true',
      help='Force the symbolic link to be created, removing existing files and '
      'directories if needed.')
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-a',
      '--use_absolute_path',
      action='store_true',
      help='Generated symlink is stored as an absolute path.')
  group.add_argument(
      '-r',
      '--use_relative_path',
      action='store_true',
      help='Generated symlink is stored as a relative path.')
  return parser


def main():
  util.SetupDefaultLoggingConfig()
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  target_path, link_path = args.link

  if args.force:
    Rmtree(link_path)
  if args.use_absolute_path:
    target_path = os.path.abspath(target_path)
  elif args.use_relative_path:
    # os.path.relpath() requires its second parameter be a directory. If our
    # target is a file, i.e. the link we will be creating should be to a file,
    # we need to calculate the relative path between our target and the
    # directory that our link will reside in, not the full path to the link
    # itself. If we do not, our relative path will ascend one level too far.
    # This is visible in the following examples.
    #
    # os.path.relpath(
    #     '/target/file.txt', '/link/file.txt') = '../../target/file.txt' [bad]
    #
    # os.path.relpath(
    #     '/target/file.txt', '/link')          = '../target/file.txt'   [good]
    if os.path.isfile(target_path):
      relative_link_path = os.path.dirname(link_path)
    else:
      relative_link_path = link_path
    target_path = os.path.relpath(target_path, relative_link_path)
  MakeSymLink(target_path=target_path, link_path=link_path)


if __name__ == '__main__':
  main()
