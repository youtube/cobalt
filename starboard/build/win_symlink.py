#!/usr/bin/python
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

"""
This file provides functions that provide symlinking of folders. This is
necessary because os.symlink in python 2.7 is missing for Windows.

Reparse points: Are os-level symlinks for folders which can be created without
admin access. Symlinks for folders are supported using this mechanism. Note
that reparse points require special care for traversal, because reparse points
are often skipped or treated as files by the various python path manipulation
functions in os and shutil modules. rmtree() as a replacement for
shutil.rmtree() is provided.

Junction points: Are low level file system symlinks. We do not support these
yet because creating them requires admin level acccess, or Windows 10 Insiders
build 14972, which is not widely available yet.
"""

import os
import shutil
import subprocess
import traceback


def CreateReparsePoint(from_folder, link_folder):
  """ Mimics os.symlink for usage. """
  return _CreateReparsePoint(from_folder, link_folder)


def ReadReparsePoint(path):
  """ Mimics os.readlink for usage. """
  return _ReadReparsePoint(path)


def IsReparsePoint(path):
  """ Mimics os.islink for usage. """
  return _IsReparsePoint(path)


def UnlinkReparsePoint(link_dir):
  """ Mimics os.unlink for usage. The sym link_dir is removed."""
  return _UnlinkReparsePoint(link_dir)


def Rmtree(dirpath):
  """ Like shutil.rmtree but follows reparse points"""
  return _Rmtree(dirpath)


#####################
# Implementation
#####################


def _Rmtree(dirpath):
  delete_path = dirpath
  reparse_path = _ReadReparsePoint(dirpath)
  delete_path = reparse_path if reparse_path else dirpath
  if not delete_path:
    return
  if not os.path.exists(delete_path):
    raise IOError("path " + delete_path + " does not exist.")
  for path in os.listdir(delete_path):
    fullpath = os.path.join(delete_path, path)
    if os.path.isdir(fullpath):
      _Rmtree(fullpath)
    elif os.path.isfile(fullpath):
      os.remove(fullpath)
    else:
      child_reparse_path = _ReadReparsePoint(fullpath)
      if child_reparse_path:
        _Rmtree(child_reparse_path)
      else:
        raise IOError("Unknown path type: " + fullpath)
  if reparse_path:
    UnlinkReparsePoint(dirpath)
  if os.path.isdir(delete_path):
    try:
      os.removedirs(delete_path)
    except Exception as err:
      print("Error while removing " + delete_path \
            + " because " + str(err))


def _ReadReparsePoint(path):
  path = os.path.abspath(path)
  cmd_parts = ['fsutil', 'reparsepoint', 'query', path]
  try:
    out = subprocess.check_output(cmd_parts)
  except subprocess.CalledProcessError:
    # Expected if the link doesn't exist.
    return None
  try:
    lines = out.splitlines()
    lines = [ l for l in lines if "Print Name:" in l ]
    if not lines:
      return None
    out = lines[0].split()
    return out[2]
  except Exception as err:
    traceback.print_exc()
    print err
    return None


def _IsReparsePoint(path):
  return None != _ReadReparsePoint(path)


def _CreateReparsePoint(from_folder, link_folder):
  from_folder = os.path.abspath(from_folder)
  link_folder = os.path.abspath(link_folder)
  if os.path.isdir(link_folder):
    os.removedirs(link_folder)
  else:
    _UnlinkReparsePoint(link_folder)  # Deletes if it exists.
  cmd_parts = ['mklink', '/j', link_folder, from_folder]
  # Shell is required for reparse point creation.
  subprocess.check_output(' '.join(cmd_parts), shell=True)


def _UnlinkReparsePoint(link_dir):
  if not _IsReparsePoint(link_dir):
    return
  cmd_parts = ['fsutil', 'reparsepoint', 'delete', link_dir]
  subprocess.check_output(cmd_parts)
  # The folder will now be unlinked, but will still exist.
  if os.path.isdir(link_dir):
    try:
      os.removedirs(link_dir)
    except Exception as err:
      print(__file__ + " could not remove " + link_dir)
      print(str(err))
  if _IsReparsePoint(link_dir):
    raise IOError("Link still exists: " + _ReadReparsePoint(link_dir))
  if os.path.isdir(link_dir):
    raise IOError("Link as folder still exists: " + link_dir)


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
  tmp_dir = os.path.join(os.environ['temp'], 'win_symlink')
  from_dir = os.path.join(tmp_dir, 'from_dir')
  test_txt = os.path.join(from_dir, 'test.txt')
  inner_dir = os.path.join(from_dir, 'inner_dir')
  link_dir = os.path.join(tmp_dir, 'link')
  link_dir2 = os.path.join(tmp_dir, 'link2')
  if IsReparsePoint(link_dir):
    print "Deleting previous link_dir:", link_dir
    UnlinkReparsePoint(link_dir)
  else:
    print "Previous link dir does not exist."
  print "from_dir:", os.path.abspath(from_dir)
  print "link_dir:", os.path.abspath(link_dir)
  print "link_dir exists? ", _ReadReparsePoint(link_dir)

  if not os.path.isdir(from_dir):
    os.makedirs(from_dir)
  if not os.path.isdir(inner_dir):
    os.makedirs(inner_dir)
  with open(test_txt, 'w') as fd:
    fd.write('hello world')

  # Check that the ReadReparsePoint handles non link objects ok.
  if ReadReparsePoint(from_dir):
    raise IOError("Exepected ReadReparsePoint() to return None for " + from_dir)
  if ReadReparsePoint(test_txt):
    raise IOError("Exepected ReadReparsePoint() to return None for " + test_txt)

  CreateReparsePoint(from_dir, link_dir)

  link_created_ok = IsReparsePoint(link_dir)
  if link_created_ok:
    print("Link created: " + str(link_created_ok))
  else:
    raise IOError("Failed to create link " + link_dir)

  CreateReparsePoint(from_dir, link_dir2)
  if not IsReparsePoint(link_dir2):
    raise IOError("Failed to create link " + link_dir2)
  UnlinkReparsePoint(link_dir2)
  if os.path.exists(link_dir2):
    raise IOError("Still exists: " + link_dir2)

  from_dir_2 = ReadReparsePoint(link_dir)
  if _IsSamePath(from_dir_2, from_dir):
    print "Link exists."
  else:
    raise IOError("Link mismatch: " + from_dir_2 + ' != ' + from_dir)

  Rmtree(link_dir)
  if os.path.exists(link_dir):
    raise IOError("Link dir " + link_dir + " still exists.")
  if os.path.exists(from_dir):
    raise IOError("From Dir " + from_dir + " still exits.")
  print "Test completed."


if __name__ == "__main__":
  UnitTest()
