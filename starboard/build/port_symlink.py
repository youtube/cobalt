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


################################################################################
#                                  API                                         #
################################################################################


def IsWindows():
  return _IsWindows()


# Platform neutral version os os.path.islink()
def IsSymLink(path):
  return _IsSymLink(path=path)


def MakeSymLink(from_folder, link_folder):
  """Makes a symlink
    Input:
      link_folder: Path to the link
      from_folder: Path to the actual folder
  """
  return _MakeSymLink(from_folder=from_folder, link_folder=link_folder)


def ReadSymLink(link_path):
  return _ReadSymLink(link_path=link_path)


def DelSymLink(link_path):
  return _DelSymLink(link_path=link_path)


def Rmtree(path):
  return _Rmtree(path=path)


def OsWalk(root_dir, topdown=True, onerror=None, followlinks=False):
  return _OsWalk(root_dir=root_dir,
                 topdown=topdown,
                 onerror=onerror,
                 followlinks=followlinks)


################################################################################
#                                 IMPL                                         #
################################################################################

import argparse
import logging
import os
import shutil
import sys
import tempfile

import _env


def _IsWindows():
  return sys.platform in ['win32', 'cygwin']


def _IsSymLink(path):
  if _IsWindows():
    from starboard.build.win_symlink import IsReparsePoint
    return IsReparsePoint(path)
  else:
    return os.path.islink(path)


def _MakeSymLink(from_folder, link_folder):
  if _IsWindows():
    from starboard.build.win_symlink import CreateReparsePoint
    CreateReparsePoint(from_folder, link_folder)
  else:
    os.symlink(from_folder, link_folder)


def _ReadSymLink(link_path):
  """Returns a absolute path to the folder referred to by the link_path."""
  if IsWindows():
    from starboard.build.win_symlink import ReadReparsePoint
    path = ReadReparsePoint(link_path)
  else:
    try:
      path = os.readlink(link_path)
    except OSError:
      path = None
  if path and '..' in path:
    path = os.path.abspath(os.path.normpath(os.path.join(link_path, path)))
  return path


def _DelSymLink(link_path):
  if IsWindows():
    from starboard.build.win_symlink import UnlinkReparsePoint
    UnlinkReparsePoint(link_path)
  else:
    os.unlink(link_path)



def _Rmtree(path):
  if not os.path.exists(path):
    return
  if _IsWindows():
    from starboard.build.win_symlink import RmtreeShallow
    RmtreeShallow(path)
  else:
    if os.path.islink(path):
      os.unlink(path)
    else:
      shutil.rmtree(path)

def _OsWalk(root_dir, topdown=True, onerror=None, followlinks=False):
  if IsWindows():
    from starboard.build.win_symlink import OsWalk
    return OsWalk(root_dir, topdown, onerror, followlinks)
  else:
    return os.walk(root_dir, topdown, onerror, followlinks)


def _IsSamePath(p1, p2):
  if not p1:
    p1 = None
  if not p2:
    p2 = None
  if p1 == p2:
    return True
  if (not p1) or (not p2):
    return False
  p1 = os.path.abspath(os.path.normpath(p1))
  p2 = os.path.abspath(os.path.normpath(p2))
  if p1 == p2:
    return True
  try:
    return os.stat(p1) == os.stat(p2)
  except:
    return False


def UnitTest():
  """Tests that a small directory hierarchy can be created and then symlinked,
  and then removed."""
  tmp_dir = os.path.join(tempfile.gettempdir(), 'port_symlink')
  from_dir = os.path.join(tmp_dir, 'from_dir')
  test_txt = os.path.join(from_dir, 'test.txt')
  inner_dir = os.path.join(from_dir, 'inner_dir')
  link_dir = os.path.join(tmp_dir, 'link')
  link_dir2 = os.path.join(tmp_dir, 'link2')
  if IsSymLink(link_dir):
    print "Deleting previous link_dir:", link_dir
    DelSymLink(link_dir)
  else:
    print "Previous link dir does not exist."
  print "from_dir:", os.path.abspath(from_dir)
  print "link_dir:", os.path.abspath(link_dir)
  print "link_dir exists? ", ReadSymLink(link_dir)

  if not os.path.isdir(from_dir):
    os.makedirs(from_dir)
  if not os.path.isdir(inner_dir):
    os.makedirs(inner_dir)
  with open(test_txt, 'w') as fd:
    fd.write('hello world')

  # Check that the ReadReparsePoint handles non link objects ok.
  if ReadSymLink(from_dir):
    raise IOError("Exepected ReadSymLink() to return None for " + from_dir)
  if ReadSymLink(test_txt):
    raise IOError("Exepected ReadSymLink() to return None for " + test_txt)

  MakeSymLink(from_dir, link_dir)

  link_created_ok = IsSymLink(link_dir)
  if link_created_ok:
    print("Link created: " + str(link_created_ok))
  else:
    raise IOError("Failed to create link " + link_dir)

  if not os.path.exists(link_dir):
    raise IOError('os.path.exists(link_dir) is False.')

  MakeSymLink(from_dir, link_dir2)
  if not IsSymLink(link_dir2):
    raise IOError("Failed to create link " + link_dir2)
  DelSymLink(link_dir2)
  if os.path.exists(link_dir2):
    raise IOError("Still exists: " + link_dir2)

  from_dir_2 = ReadSymLink(link_dir)
  if _IsSamePath(from_dir_2, from_dir):
    print "Link exists."
  else:
    raise IOError("Link mismatch: " + from_dir_2 + ' != ' + from_dir)
  def GetAllPaths(start_dir, followlinks):
    paths = []
    for root, dirs, files in OsWalk(tmp_dir, followlinks=followlinks):
      for name in files:
        path = os.path.join(root, name)
        paths.append(path)
      for name in dirs:
        path = os.path.join(root, name)
        paths.append(path)
    return paths
  def PathTypeToString(path):
    if IsSymLink(path):
      return 'link'
    if os.path.isdir(path):
      return 'dir'
    return 'file'
  paths_nofollow_links = GetAllPaths(tmp_dir, followlinks=False)
  paths_follow_links = GetAllPaths(tmp_dir, followlinks=True)
  print '\nOsWalk Follow links:'
  for path in paths_follow_links:
    print '  ' + path + ' (' + PathTypeToString(path) + ')'
  print '\nOsWalk No-Follow links:'
  for path in paths_nofollow_links:
    print '  ' + path + ' (' + PathTypeToString(path) + ')'
  print ''

  assert(link_dir in paths_nofollow_links)
  assert(link_dir in paths_follow_links)
  assert(os.path.join(link_dir, 'test.txt') in paths_follow_links)
  assert(not os.path.join(link_dir, 'test.txt') in paths_nofollow_links)
  Rmtree(link_dir)
  if os.path.exists(link_dir):
    raise IOError("Link dir " + link_dir + " still exists.")
  if not os.path.exists(from_dir):
    raise IOError("From Dir " + from_dir + " was deleted!")
  print "Test completed."


def _CreateArgumentParser():
  class MyParser(argparse.ArgumentParser):
    def error(self, message):
      sys.stderr.write('error: %s\n' % message)
      self.print_help()
      sys.exit(2)
  help_msg = (
    'Example 1:\n'
    '  python port_link.py --unit_test "actual_folder_path" "link_path"\n\n'
    'Example 2:\n'
    '  python port_link.py --link "actual_folder_path" "link_path"\n\n'
    'Example 3:\n'
    '  python port_link.py --link "../actual_folder_path" "link_path"\n\n')
  # Enables new lines in the description and epilog.
  formatter_class = argparse.RawDescriptionHelpFormatter
  parser = MyParser(epilog=help_msg, formatter_class=formatter_class)
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument('--unit_test', help='Runs a unit test.',
                     action='store_true')
  group.add_argument('--link',
                     help='Issues an scp command to upload src to remote_dst',
                     metavar='"path"',
                     nargs=2)
  return parser


def _main():
  format = '[%(filename)s:%(lineno)s:%(levelname)s] %(message)s'
  logging.basicConfig(level=logging.INFO, format=format, datefmt='%m-%d %H:%M')
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  if args.unit_test:
    UnitTest()
  else:
    folder_path, link_path = args.link
    if '.' in folder_path:
      d1 = os.path.abspath(folder_path)
    else:
      d1 = os.path.abspath(os.path.join(link_path, folder_path))
    if not os.path.isdir(d1):
      logging.warning('%s is not a directory.' % d1)
    MakeSymLink(from_folder=folder_path, link_folder=link_path)


if __name__ == "__main__":
  _main()
