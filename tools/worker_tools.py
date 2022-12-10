# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""Tools for build workers."""

import logging
import os
import platform as sys_platform
import re
import subprocess

SOURCE_DIR = r'<source_dir>'

# These files are not final build artifacts nor inetermediate build artifacts
# needed.
BUILDBOT_INTERMEDIATE_FILE_NAMES_WILDCARDS = (
    r'gypfiles',
    r'gyp-win-tool',
    r'environment.x64',
    r'environment.x86',
    r'obj',
    r'obj.host',
    r'gradle',  # Android intermediates
    SOURCE_DIR + r'lib/*.so',
    r'intermediates',
    r'*.ninja',
    r'*.ninja_deps',
    r'*.ninja_log',
    r'*.mp4',
    r'*.deb',
)

# These files are not final build artifacts. Values should match those in
# INTERMEDIATE_WILDCARD_FILE_NAMES_RE
INTERMEDIATE_FILE_NAMES_WILDCARDS = (
    r'gen',
    r'gypfiles',
    r'gyp-win-tool',
    r'environment.x64',
    r'environment.x86',
    r'obj',
    r'obj.host',
    r'gradle',  # Android intermediates
    SOURCE_DIR + r'lib/*.so',
    r'intermediates',
    r'*.ninja',
    r'*.ninja_deps',
    r'*.ninja_log',
    r'*.mp4',
    r'*.deb',
)

# This regex is the same list as above, but is suitable for regex comparison.
INTERMEDIATE_FILE_NAMES_RE = re.compile(r'^gen$|'
                                        r'^gypfiles$|'
                                        r'^gyp-win-tool$|'
                                        r'^environment.x64$|'
                                        r'^environment.x86$|'
                                        r'^obj$|'
                                        r'^obj\.host$|'
                                        r'^gradle$|'
                                        r'^lib$|'
                                        r'^intermediates$|'
                                        r'^.*\.ninja$|'
                                        r'^.*\.ninja_deps$|'
                                        r'^.*\.ninja_log$|'
                                        r'^.*\.mp4$|'
                                        r'^.*\.deb$')

_SYMLINK_DIR = r'C:\\w'
_RM_LINK = ['cmd', '/c', 'rmdir', _SYMLINK_DIR]
_CREATE_LINK = ['cmd', '/c', 'mklink', '/d', _SYMLINK_DIR]
_DEFAULT_WORKDIR = 'workdir'


def FindWorkDir():
  """Looking for workdir in current working dir paths."""
  cur_dir = os.getcwd()
  while cur_dir:
    cur_dir, tail = os.path.split(cur_dir)
    if not tail:
      return None
    if tail == _DEFAULT_WORKDIR:
      return os.path.join(cur_dir, tail)
  return None


def MoveToWindowsShortSymlink():
  r"""Changes current work dir on Dockerized Windows to C:\w.

  This is workaround to issue of long windows path (longer than 460 symbols).
  When we could address this issue in our python scripts some tooling is not
  compatible with long paths.

  Creating symlink only if workdir is present in current path. Changing dir to
  the same level but under short link.

  Returns:
    current work dir prior to symlink creation
  """
  original_cur_dir = os.getcwd()
  if os.environ.get('IS_BUILDBOT_DOCKER') != '1':
    return original_cur_dir
  if sys_platform.system() != 'Windows':
    return original_cur_dir
  work_dir = FindWorkDir()
  if not work_dir:
    logging.warning(
        'Running from outside of default workdir: %s, path'
        'shortening skipped.', original_cur_dir)
    return original_cur_dir

  logging.info('Changing workdir to symlink %s', _SYMLINK_DIR)
  if os.path.exists(_SYMLINK_DIR):
    subprocess.call(_RM_LINK)
  subprocess.call(_CREATE_LINK + [work_dir])
  short_cur_path = GetPathUnderShortSymlink(work_dir, original_cur_dir)
  logging.info('Continuing running from: %s', short_cur_path)
  os.chdir(short_cur_path)
  return original_cur_dir


def GetPathUnderShortSymlink(workdir_path, path):
  """Returns path relative to _SYMLINK_DIR if it was under work_dir."""
  if workdir_path in path:
    return path.replace(workdir_path, _SYMLINK_DIR)
  return path


def IsIntermediateFile(file_name):
  """Return True if file is used to create artifacts and is not one itself."""
  return bool(INTERMEDIATE_FILE_NAMES_RE.match(file_name))


class TestTypes(object):
  UNIT_TEST = 'unit_test'
  BLACK_BOX_TEST = 'black_box_test'
  EVERGREEN_TEST = 'evergreen_test'
  PERFORMANCE_TEST = 'performance_test'
  ENDURANCE_TEST = 'endurance_test'
  INTEGRATION_TEST = 'integration_test'
  UPLOAD_ARCHIVE = 'upload_archive'

  VALID_TYPES = [
      UNIT_TEST, BLACK_BOX_TEST, PERFORMANCE_TEST, EVERGREEN_TEST,
      ENDURANCE_TEST, INTEGRATION_TEST, UPLOAD_ARCHIVE
  ]
