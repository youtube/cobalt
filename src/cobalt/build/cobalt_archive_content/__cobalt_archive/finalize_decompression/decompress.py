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


"""This file is designed to be placed in the cobalt archive to finalize the
decompression process."""

import json
import logging
import os
import subprocess
import sys


_SELF_DIR = os.path.dirname(__file__)
_ARCHIVE_ROOT = os.path.join(_SELF_DIR, os.pardir, os.pardir)
_DATA_JSON_PATH = os.path.abspath(os.path.join(_SELF_DIR, 'decompress.json'))


def _DefineOsSymlinkForWin32():
  """When invoked, this will define the missing os.symlink for Win32."""
  def _CreateWin32Symlink(source, link_name):
    cmd = 'mklink /J %s %s' % (link_name, source)
    rc = subprocess.call(cmd, shell=True)
    if rc != 0:
      logging.critical('Error using %s during %s, cwd=%s' % (rc,cmd,os.getcwd()))
  os.symlink = _CreateWin32Symlink


if sys.platform == 'win32':
  _DefineOsSymlinkForWin32()


def _ExtractSymlinks(cwd, symlink_dir_list):
  if not symlink_dir_list:
    logging.info('No directory symlinks found.')
    return
  prev_cwd = os.getcwd()
  cwd = os.path.normpath(cwd)
  all_ok = True
  try:
    os.chdir(cwd)
    for link_path, real_path in symlink_dir_list:
      link_path = os.path.abspath(link_path)
      real_path = os.path.abspath(real_path)
      if not os.path.exists(real_path):
        os.makedirs(real_path)
      if not os.path.exists(os.path.dirname(link_path)):
        os.makedirs(os.path.dirname(link_path))
      assert(os.path.exists(real_path))
      real_path = os.path.relpath(real_path)
      os.symlink(real_path, link_path)
      if not os.path.exists(real_path):
        logging.critical('Error target folder %s does not exist.'
                         % os.path.abspath(real_path))
        all_ok = False
      if not os.path.exists(link_path):
        logging.critical('Error link folder %s does not exist.'
                         % os.path.abspath(link_path))
        all_ok = False
  finally:
    os.chdir(prev_cwd)
  if not all_ok:
    logging.critical('\n*******************************************'
                     '\nErrors happended during symlink extraction.'
                     '\n*******************************************')


def _main():
  logging.basicConfig(level=logging.INFO,
                      format='%(filename)s(%(lineno)s): %(message)s')
  assert(os.path.exists(_DATA_JSON_PATH)), _DATA_JSON_PATH
  with open(_DATA_JSON_PATH) as fd:
    json_str = fd.read()
  data = json.loads(json_str)
  _ExtractSymlinks(_ARCHIVE_ROOT, data.get('symlink_dir', []))


if __name__ == '__main__':
  _main()
