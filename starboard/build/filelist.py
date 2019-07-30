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


"""A utility for generating a file list.

Each entry in the FileList contains the file path and a path relative to the
root directory that can be used for creating archives.

"""

import logging
import os

import _env  # pylint: disable=relative-import,unused-import

from starboard.build import port_symlink


class FileList(object):
  """Makes it easy to include files for things like archive operations."""

  def __init__(self):
    self.file_list = []  # List of 2-tuples: file_path, archive_path
    self.symlink_dir_list = []  # Same

  def AddAllFilesInPath(self, root_dir, sub_path):
    """Starting at the root path, the sub_paths are searched for files."""
    all_files = []
    all_symlinks = []
    if os.path.isfile(sub_path):
      self.AddFile(root_dir, sub_path)
    elif not os.path.isdir(sub_path):
      raise IOError('Expected root directory to exist: %s' % sub_path)
    cwd = os.getcwd()
    for root, dirs, files in port_symlink.OsWalk(sub_path):
      # Do not use os.path.abspath as it does not work on Win for long paths.
      if not os.path.isabs(root):
        root = os.path.join(cwd, root)
      for f in files:
        full_path = os.path.join(root, f)
        all_files.append(full_path)
      for dir_name in dirs:
        full_path = os.path.join(root, dir_name)
        if port_symlink.IsSymLink(full_path):
          all_symlinks.append(full_path)
    for f in all_files + all_symlinks:
      self.AddFile(root_dir, f)

  def AddAllFilesInPaths(self, root_dir, sub_paths):
    if not os.path.isdir(root_dir):
      raise IOError('Expected root directory to exist: ' + str(root_dir))
    for p in sub_paths:
      self.AddAllFilesInPath(root_dir=root_dir, sub_path=p)

  def AddFile(self, root_path, file_path):
    if port_symlink.IsSymLink(file_path):
      self.AddSymLink(root_path, file_path)
    else:
      archive_name = _OsGetRelpath(file_path, root_path)
      self.file_list.append([file_path, archive_name])

  def AddSymLink(self, relative_dir, link_file):
    rel_link_path = _OsGetRelpath(link_file, relative_dir)
    target_path = _ResolveSymLink(link_file)
    assert os.path.exists(target_path)
    rel_target_path = _OsGetRelpath(target_path, relative_dir)
    self.symlink_dir_list.append([relative_dir, rel_link_path, rel_target_path])

  def Print(self):
    for f in self.file_list:
      print 'File:    %s' % f
    for s in self.symlink_dir_list:
      print 'Symlink: %s' % s


def _ResolveSymLink(link_file):
  """Returns the absolute path of the resolved link. This path should exist."""
  target_path = port_symlink.ReadSymLink(link_file)
  if os.path.isabs(target_path):  # Absolute path
    assert os.path.exists(target_path), (
        'Path {} does not exist.'.format(target_path))
    return target_path
  else:  # Relative path from link_file.
    abs_path = os.path.normpath(os.path.join(link_file, target_path))
    assert os.path.exists(abs_path), (
        'Path {} does not exist (link file: {}, target_path: {})'.format(
            abs_path, link_file, target_path))
    return abs_path


def _FallbackOsGetRelPath(path, start_dir):
  path = os.path.normpath(path)
  start_dir = os.path.normpath(start_dir)
  common_prefix = os.path.commonprefix([path, start_dir])
  split_list = common_prefix.split(os.sep)
  path_parts = path.split(os.sep)
  for _ in range(len(split_list)):
    path_parts = path_parts[1:]
  return os.path.normpath(os.path.join(*path_parts))


def _OsGetRelpath(path, start_dir):
  path = os.path.normpath(path)
  start_dir = os.path.normpath(start_dir)
  # Use absolute paths for Windows (nt paths checks the drive specifier).
  # Do not use os.path.abspath as it does not work on Win for long paths.
  if not os.path.isabs(path):
    path = os.path.join(os.getcwd(), path)
  if not os.path.isabs(start_dir):
    start_dir = os.path.join(os.getcwd(), start_dir)
  try:
    return os.path.relpath(path, start_dir)
  except ValueError:
    try:
      # Do a string comparison to get relative path.
      # Fixes issue b/134589032
      rel_path = _FallbackOsGetRelPath(path, start_dir)
      if not os.path.exists(os.path.join(start_dir, rel_path)):
        raise ValueError('%s does not exist.' % os.path.abspath(rel_path))
      return rel_path
    except ValueError as err:
      logging.exception('Error %s while calling os.path.relpath(%s, %s)',
                        err, path, start_dir)


TYPE_NONE = 'NONE'
TYPE_SYMLINK_DIR = 'SYMLINK DIR'
TYPE_DIRECTORY = 'DIR'
TYPE_FILE = 'FILE'


def GetFileType(f):
  if not os.path.exists(f):
    return TYPE_NONE
  elif port_symlink.IsSymLink(f):
    return TYPE_SYMLINK_DIR
  elif os.path.isdir(f):
    return TYPE_DIRECTORY
  else:
    assert os.path.isfile(f)
    return TYPE_FILE
