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


def IsSymLink(path):
  """Platform neutral version os os.path.islink()"""
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    return win_symlink.IsReparsePoint(path)
  else:
    return os.path.islink(path)


def MakeSymLink(from_folder, link_folder):
  """Makes a symlink.

  Args:
    from_folder: Path to the actual folder
    link_folder: Path to the link

  Returns:
    None
  """
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    win_symlink.CreateReparsePoint(from_folder, link_folder)
  else:
    util.MakeDirs(os.path.dirname(link_folder))
    os.symlink(from_folder, link_folder)


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
  if not os.path.exists(path):
    return
  if IsWindows():
    # pylint: disable=g-import-not-at-top
    from starboard.tools import win_symlink
    win_symlink.RmtreeShallow(path)
  else:
    if os.path.islink(path):
      os.unlink(path)
    else:
      shutil.rmtree(path)


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
  help_msg = (
      'Example 1:\n'
      '  python port_link.py --link "actual_folder_path" "link_path"\n\n'
      'Example 2:\n'
      '  python port_link.py --link "../actual_folder_path" "link_path"\n\n')
  # Enables new lines in the description and epilog.
  formatter_class = argparse.RawDescriptionHelpFormatter
  parser = MyParser(epilog=help_msg, formatter_class=formatter_class)
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument('--link',
                     help='Issues an scp command to upload src to remote_dst',
                     metavar='"path"',
                     nargs=2)
  return parser


def main():
  util.SetupDefaultLoggingConfig()
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  folder_path, link_path = args.link
  if '.' in folder_path:
    d1 = os.path.abspath(folder_path)
  else:
    d1 = os.path.abspath(os.path.join(link_path, folder_path))
  if not os.path.isdir(d1):
    logging.warning('%s is not a directory.', d1)
  MakeSymLink(from_folder=folder_path, link_folder=link_path)


if __name__ == '__main__':
  main()
