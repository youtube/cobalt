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

"""Provides functions for symlinking on Windows.

Reparse points: Are os-level symlinks for folders which can be created without
admin access. Symlinks for folders are supported using this mechanism. Note
that reparse points require special care for traversal, because reparse points
are often skipped or treated as files by the various python path manipulation
functions in os and shutil modules. rmtree() as a replacement for
shutil.rmtree() is provided.

"""

import logging
import os
import shutil
import stat
import subprocess
import time

from cobalt.build import cobalt_archive_extract


################################################################################
#                                  API                                         #
################################################################################


def CreateReparsePoint(from_folder, link_folder):
  """Mimics os.symlink for usage.

  Args:
    from_folder: Path of target directory.
    link_folder: Path to create link.

  Returns:
    None.

  Raises:
    OSError if link cannot be created
  """
  return _CreateReparsePoint(from_folder, link_folder)


def ReadReparsePoint(path):
  """Mimics os.readlink for usage."""
  return _ReadReparsePoint(path)


def IsReparsePoint(path):
  """Mimics os.islink for usage."""
  return _IsReparsePoint(path)


def UnlinkReparsePoint(link_dir):
  """Mimics os.unlink for usage. The sym link_dir is removed."""
  return _UnlinkReparsePoint(link_dir)


def RmtreeShallow(dirpath):
  """Emulates shutil.rmtree on linux.

  Will delete symlinks but doesn't follow them. Note that shutil.rmtree on
  windows will follow the symlink and delete the files in the original
  directory!

  Args:
    dirpath: The start path to delete files.
  """
  return _RmtreeShallow(dirpath)


def OsWalk(top, topdown=True, onerror=None, followlinks=False):
  """Emulates os.walk() on linux.

  Args:
    top: see os.walk(...)
    topdown: see os.walk(...)
    onerror: see os.walk(...)
    followlinks: see os.walk(...)

  Returns:
    see os.walk(...)

  Correctly handles windows reparse points as symlinks.
  All symlink directories are returned in the directory list and the caller must
  call IsReparsePoint() on the path to determine whether the directory is
  real or a symlink.
  """
  return _OsWalk(top, topdown, onerror, followlinks)


################################################################################
#                                 IMPL                                         #
################################################################################


_RETRY_TIMES = 10


def _RemoveEmptyDirectory(path):
  """Removes a directory with retry amounts."""
  for i in range(0, _RETRY_TIMES):
    try:
      os.chmod(path, stat.S_IWRITE)
      os.rmdir(path)
      return
    except Exception:  # pylint: disable=broad-except
      if i == _RETRY_TIMES-1:
        raise
      else:
        time.sleep(.1)


def _RmtreeOsWalk(root_dir):
  """Walks the directory structure to delete directories and files."""
  del_dirs = []  # Defer deletion of directories.
  if _IsReparsePoint(root_dir):
    _UnlinkReparsePoint(root_dir)
    return
  for root, dirs, files in OsWalk(root_dir, followlinks=False):
    for name in files:
      path = os.path.join(root, name)
      os.remove(path)
    for name in dirs:
      path = os.path.join(root, name)
      if _IsReparsePoint(path):
        _UnlinkReparsePoint(path)
      else:
        del_dirs.append(path)
  # At this point, all files should be deleted and all symlinks should be
  # unlinked.
  for d in del_dirs + [root_dir]:
    try:
      if os.path.isdir(d):
        shutil.rmtree(d)
    except Exception as err:  # pylint: disable=broad-except
      logging.exception('Error while deleting: %s', err)


def _RmtreeShellCmd(root_dir):
  subprocess.call(['cmd', '/c', 'rmdir', '/S', '/Q', root_dir])


def _RmtreeShallow(root_dir):
  """See RmtreeShallow() for documentation."""
  try:
    # This can fail if there are very long file names.
    _RmtreeOsWalk(root_dir)
  except OSError:
    # This fallback will handle very long file. Note that it is VERY slow
    # in comparison to the _RmtreeOsWalk() version.
    _RmtreeShellCmd(root_dir)
  if os.path.isdir(root_dir):
    logging.error('Directory %s still exists.', root_dir)


def _ReadReparsePointShell(path):
  """Implements reading a reparse point via a shell command."""
  cmd_parts = ['fsutil', 'reparsepoint', 'query', path]
  try:
    out = subprocess.check_output(cmd_parts)
  except subprocess.CalledProcessError:
    # Expected if the link doesn't exist.
    return None
  try:
    lines = out.splitlines()
    lines = [l for l in lines if 'Print Name:' in l]
    if not lines:
      return None
    out = lines[0].split()
    return out[2]
  except Exception as err:  # pylint: disable=broad-except
    logging.exception(err)
    return None


def _ReadReparsePoint(path):
  try:
    # pylint: disable=g-import-not-at-top
    import win_symlink_fast
    return win_symlink_fast.FastReadReparseLink(path)
  except Exception as err:  # pylint: disable=broad-except
    logging.exception(' error: %s, falling back to command line version.', err)
    return _ReadReparsePointShell(path)


def _IsReparsePoint(path):
  try:
    # pylint: disable=g-import-not-at-top
    import win_symlink_fast
    return win_symlink_fast.FastIsReparseLink(path)
  except  Exception as err:  # pylint: disable=broad-except
    logging.exception(' error: %s, falling back to command line version.', err)
    return None is not _ReadReparsePointShell(path)


def _CreateReparsePoint(from_folder, link_folder):
  """See api version above for doc string."""
  if os.path.isdir(link_folder):
    _RemoveEmptyDirectory(link_folder)
  else:
    _UnlinkReparsePoint(link_folder)  # Deletes if it exists.
  try:
    # pylint: disable=g-import-not-at-top
    import win_symlink_fast
    win_symlink_fast.FastCreateReparseLink(from_folder, link_folder)
    return
  except OSError:
    pass
  except Exception as err:  # pylint: disable=broad-except
    logging.exception('unexpected error: %s, from=%s, link=%s, falling back to '
                      'command line version.', err, from_folder, link_folder)
  par_dir = os.path.dirname(link_folder)
  if not os.path.isdir(par_dir):
    os.makedirs(par_dir)
  try:
    subprocess.check_output(
        ['cmd', '/c', 'mklink', '/d', link_folder, from_folder],
        stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError:
    # Fallback to junction points, which require less privileges to create.
    subprocess.check_output(
        ['cmd', '/c', 'mklink', '/j', link_folder, from_folder])
  if not _IsReparsePoint(link_folder):
    raise OSError('Could not create sym link %s to %s' %
                  (link_folder, from_folder))


def _UnlinkReparsePoint(link_dir):
  """See api above for docstring."""
  if not _IsReparsePoint(link_dir):
    return
  cmd_parts = ['fsutil', 'reparsepoint', 'delete', link_dir]
  subprocess.check_output(cmd_parts)
  # The folder will now be unlinked, but will still exist.
  if os.path.isdir(link_dir):
    try:
      _RemoveEmptyDirectory(link_dir)
    except Exception as err:  # pylint: disable=broad-except
      logging.exception('could not remove %s because of %s', link_dir, err)
  if _IsReparsePoint(link_dir):
    raise IOError('Link still exists: %s' % _ReadReparsePoint(link_dir))
  if os.path.isdir(link_dir):
    logging.info('WARNING - Link as folder still exists: %s', link_dir)


def _IsSamePath(p1, p2):
  """Returns true if p1 and p2 represent the same path."""
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
  except Exception:  # pylint: disable=broad-except
    return False


def _OsWalk(top, topdown, onerror, followlinks):
  """See api version of OsWalk above, for docstring."""
  # Need an absolute path to use listdir and isdir with long paths.
  top_abs_path = top
  if not os.path.isabs(top_abs_path):
    top_abs_path = os.path.join(os.getcwd(), top_abs_path)
  top_abs_path = cobalt_archive_extract.ToWinUncPath(top)
  try:
    names = os.listdir(top_abs_path)
  except OSError as err:
    if onerror is not None:
      onerror(err)
    return
  dirs, nondirs = [], []
  for name in names:
    if os.path.isdir(os.path.join(top_abs_path, name)):
      dirs.append(name)
    else:
      nondirs.append(name)
  if topdown:
    yield top, dirs, nondirs
  for name in dirs:
    new_path = os.path.join(top, name)
    if followlinks or not _IsReparsePoint(new_path):
      for x in _OsWalk(new_path, topdown, onerror, followlinks):
        yield x
  if not topdown:
    yield top, dirs, nondirs
