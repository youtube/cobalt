#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
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
"""Utilities for use by gyp_cobalt and other build tools."""

import importlib
import logging
import os
import re
import subprocess
import sys


_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
_SOURCE_TREE_DIR = os.path.abspath(os.path.join(_SCRIPT_DIR, os.pardir,
                                                os.pardir))


def GetGyp():
  if 'gyp' not in sys.modules:
    # Add directory to path to make sure that cygpath can be imported.
    sys.path.insert(0, os.path.join(_SOURCE_TREE_DIR, 'lbshell', 'build',
                                    'pylib'))
    sys.path.insert(0, os.path.join(_SOURCE_TREE_DIR, 'tools', 'gyp', 'pylib'))
    importlib.import_module('gyp')
  return sys.modules['gyp']


def GetSourceTreeDir():
  return _SOURCE_TREE_DIR


def GypDebugOptions():
  """Returns all valid GYP debug options."""
  debug_modes = []
  gyp = GetGyp()
  for name in dir(gyp):
    if name.startswith('DEBUG_'):
      debug_modes.append(getattr(gyp, name))
  return debug_modes


def GitHash(path_to_git_repo):
  """Compute HEAD SHA1 of the git repo that contains the given path."""

  full_path = os.path.join(_SOURCE_TREE_DIR, path_to_git_repo)
  if not os.path.exists(full_path):
    logging.error('Unable to query git hash. '
                  'Path does not exist "%s".', full_path)
    return '0'

  current_dir = os.getcwd()
  try:
    os.chdir(full_path)
    cmd_line = ('git log --pretty=format:%h -n 1')
    sp = subprocess.check_output(cmd_line, shell=True)
    return sp.strip()
  except subprocess.CalledProcessError:
    logging.error('Unable to query git hash.')
    return '0'
  finally:
    os.chdir(current_dir)


def Which(filename):
  for path in os.environ['PATH'].split(os.pathsep):
    full_name = os.path.join(path, filename)
    if os.path.exists(full_name) and os.path.isfile(full_name):
      return full_name
  return None


def _EnsureGomaRunning():
  """Ensure goma is running."""

  cmd_line = ['goma_ctl.py', 'ensure_start']
  try:
    subprocess.check_output(cmd_line, shell=True, stderr=subprocess.STDOUT)
    return True
  except subprocess.CalledProcessError as e:
    logging.error('goma failed to start.\nCommand: %s\n%s',
                  ' '.join(e.cmd), e.output)
    return False


def FindAndInitGoma():
  """Checks if goma is installed and makes sure that it's initialized.

  Returns:
    True if goma was found and correctly initialized.
  """

  # GOMA is only supported on Linux.
  if not sys.platform.startswith('linux'):
    return False

  # See if we can find gomacc somewhere the path.
  gomacc_path = Which('gomacc')

  if 'USE_GOMA' in os.environ:
    use_goma = int(os.environ['USE_GOMA'])
    if use_goma and not gomacc_path:
      logging.critical('goma was requested but gomacc not found in PATH.')
      sys.exit(1)
  else:
    use_goma = bool(gomacc_path)

  if use_goma:
    use_goma = _EnsureGomaRunning()
  return use_goma


def GetConstantValue(file_path, constant_name):
  """Return an rvalue from a C++ header file.

  Search a C/C++ header file at |file_path| for an assignment to the
  constant named |constant_name|. The rvalue for the assignment must be a
  literal constant or an expression of literal constants.

  Args:
    file_path: Header file to search.
    constant_name: Name of C++ rvalue to evaluate.

  Returns:
    constant_name as an evaluated expression.
  """

  search_re = re.compile(r'%s\s*=\s*([\d\+\-*/\s\(\)]*);' % constant_name)
  with open(file_path, 'r') as f:
    match = search_re.search(f.read())

  if not match:
    logging.critical('Could not query constant value.  The expression '
                     'should only have numbers, operators, spaces, and '
                     'parens.  Please check "%s" in %s.\n',
                     constant_name, file_path)
    sys.exit(1)

  expression = match.group(1)
  value = eval(expression)
  return value


def GetStackSizeConstant(platform, constant_name):
  """Gets a constant value from lb_shell_constants.h."""
  # TODO(***REMOVED***): This needs to be reimplemented if necessary. For now,
  # it'll return the default value.
  _, _ = platform, constant_name
  return 0
