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


"""Finalizes decompression.

This is meant to be run after the zip is decompressed into the temp directory
and includes support for special file system operations not supported by the
zip file.
"""


import json
import logging
import os
import subprocess
import sys


_SELF_DIR = os.path.dirname(__file__)
_ARCHIVE_ROOT = os.path.join(_SELF_DIR, os.pardir, os.pardir)
_DATA_JSON_PATH = os.path.abspath(os.path.join(_SELF_DIR, 'decompress.json'))
_FULL_PERMISSIONS = 0o777


_IS_WINDOWS = sys.platform in ['win32', 'cygwin']


def _CreateWin32Symlink(source, link_name):
  rc = subprocess.call('mklink /D %s %s' % (link_name, source), shell=True)
  if rc != 0:
    # Some older versions of windows require admin permissions for /D style
    # reparse points. In this case fallback to using /J.
    cmd = 'mklink /J %s %s' % (link_name, source),
    rc = subprocess.call(cmd, shell=True)
    if rc != 0:
      logging.critical('Error using %s during %s, cwd=%s', rc, cmd, os.getcwd())


def _CreateSymlink(source, link_name):
  if _IS_WINDOWS:
    _CreateWin32Symlink(source, link_name)
  else:
    os.symlink(source, link_name)


def _MakeDirs(path):
  if _IS_WINDOWS:
    # Necessary for long file name support
    subprocess.check_call('mkdir %s' % path, shell=True)
  else:
    os.makedirs(path)


def _ExtractSymlinks(cwd, symlink_dir_list):
  """Recreates symlinks on Windows and linux."""
  prev_cwd = os.getcwd()
  cwd = os.path.normpath(cwd)
  all_ok = True
  try:
    os.chdir(cwd)
    for link_path, real_path in symlink_dir_list:
      link_path = os.path.abspath(link_path)
      real_path = os.path.abspath(real_path)
      if not os.path.exists(real_path):
        _MakeDirs(real_path)
      if not os.path.exists(os.path.dirname(link_path)):
        _MakeDirs(os.path.dirname(link_path))
      assert os.path.exists(real_path)
      real_path = os.path.relpath(real_path)
      _CreateSymlink(real_path, link_path)
      if not os.path.exists(real_path):
        logging.critical('Error target folder %s does not exist.',
                         os.path.abspath(real_path))
        all_ok = False
      if not os.path.exists(link_path):
        logging.critical('Error link folder %s does not exist.',
                         os.path.abspath(link_path))
        all_ok = False
  finally:
    os.chdir(prev_cwd)
  if not all_ok:
    logging.critical('\n*******************************************'
                     '\nErrors happended during symlink extraction.'
                     '\n*******************************************')


def _SetExecutionBits(cwd, executable_files):
  if not executable_files:
    return
  logging.info('Setting Permissions %s on %d files',
               _FULL_PERMISSIONS, len(executable_files))
  for f in executable_files:
    full_path = os.path.abspath(os.path.join(cwd, f))
    logging.info('  %s', full_path)
    os.chmod(full_path, _FULL_PERMISSIONS)


def main():
  logging.basicConfig(level=logging.INFO,
                      format='%(filename)s(%(lineno)s): %(message)s')
  assert(os.path.exists(_DATA_JSON_PATH)), _DATA_JSON_PATH
  with open(_DATA_JSON_PATH) as fd:
    json_str = fd.read()
  data = json.loads(json_str)
  _ExtractSymlinks(_ARCHIVE_ROOT, data.get('symlink_dir', []))
  _SetExecutionBits(_ARCHIVE_ROOT, data.get('executable_files', []))


if __name__ == '__main__':
  main()
