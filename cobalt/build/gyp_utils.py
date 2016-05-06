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
import json
import logging
import os
import re
import subprocess
import sys
import urllib
import urllib2


_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
_SOURCE_TREE_DIR = os.path.abspath(os.path.join(_SCRIPT_DIR, os.pardir,
                                                os.pardir))

_VERSION_SERVER_URL = 'https://carbon-airlock-95823.appspot.com/build_version/generate'  # pylint:disable=line-too-long
_XSSI_PREFIX = ")]}'\n"

# The list of directories, relative to "src/", to search through for ports.
_PORT_SEARCH_PATH = [
    'third_party/starboard',
    'starboard',
]

# The list of files that must be present in a directory to allow it to be
# considered a valid port.
_PORT_FILES = [
    'gyp_configuration.gypi',
    'gyp_configuration.py',
    'starboard_platform.gyp',
    'configuration_public.h',
    'atomic_public.h',
    'thread_types_public.h',
]

# Whether to emit warnings if it finds a directory that almost has a port.
# TODO: Enable when tree is clean.
_WARN_ON_ALMOST_PORTS = False


# The path to the build.id file that preserves a build ID.
BUILD_ID_PATH = os.path.abspath(os.path.join(_SCRIPT_DIR, 'build.id'))


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


def GetRevinfo():
  """Get absolute state of all git repos from gclient DEPS."""

  try:
    revinfo_cmd = ['gclient', 'revinfo', '-a']

    if sys.platform.startswith('linux'):
      use_shell = False
    else:
      # Windows needs shell to find gclient in the PATH.
      use_shell = True
    output = subprocess.check_output(revinfo_cmd, shell=use_shell)
    revinfo = {}
    lines = output.splitlines()
    for line in lines:
      repo, url = line.split(':', 1)
      repo = repo.strip().replace('\\', '/')
      url = url.strip()
      revinfo[repo] = url
    return revinfo
  except subprocess.CalledProcessError as e:
    logging.warning('Failed to get revision information: %s', e)
    return {}


def GetBuildNumber(version_server=_VERSION_SERVER_URL):
  """Send a request to the build version server for a build number."""

  if os.path.isfile(BUILD_ID_PATH):
    with open(BUILD_ID_PATH, 'r') as build_id_file:
      return build_id_file.read().replace('\n', '')

  revinfo = GetRevinfo()
  json_deps = json.dumps(revinfo)
  username = os.environ.get('USERNAME', os.environ.get('USER'))

  post_data = {'deps': json_deps}
  if username:
    post_data['user'] = username

  logging.info('Post data is %s', post_data)
  request = urllib2.Request(version_server, data=urllib.urlencode(post_data))
  # TODO(***REMOVED***): retry on timeout.
  try:
    response = urllib2.urlopen(request)
  except urllib2.HTTPError as e:
    logging.warning('Failed to retrieve build number: %s', e)
    return 0
  except urllib2.URLError as e:
    logging.warning('Could not connect to %s: %s', version_server, e)
    return 0

  data = response.read()
  if data.find(_XSSI_PREFIX) == 0:
    data = data[len(_XSSI_PREFIX):]
  results = json.loads(data)
  build_number = results.get('build_number', 0)
  logging.info('Build Number: %d', build_number)
  return build_number


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
  value = eval(expression)  # pylint:disable=eval-used
  return value


def GetStackSizeConstant(platform, constant_name):
  """Gets a constant value from lb_shell_constants.h."""
  # TODO(***REMOVED***): This needs to be reimplemented if necessary. For now,
  # it'll return the default value.
  _, _ = platform, constant_name
  return 0


def _IsValidPortFilenameList(directory, filenames):
  """Determines if |filenames| contains the required files for a valid port."""

  missing = set(_PORT_FILES) - set(filenames)
  if missing:
    if len(missing) < (len(_PORT_FILES) / 2) and _WARN_ON_ALMOST_PORTS:
      logging.warning('Directory %s contains several files needed for a port, '
                      'but not all of them. In particular, it is missing: %s',
                      directory, ', '.join(missing))
  return not missing


def _GetPortName(root, directory):
  """Gets the name of a port found at |directory| off of |root|."""

  assert directory.startswith(root)
  start = len(root) + 1  # Remove the trailing slash from the root.

  assert start < len(directory)

  # Calculate the name based on relative path from search root to port.
  return re.sub(r'[^a-zA-Z0-9_]', r'_', directory[start:])


def _FindValidPorts(root, result):
  """Adds name->path for all ports under |directory| to |result|."""
  for current_path, directories, filenames in os.walk(root):
    if _IsValidPortFilenameList(current_path, filenames):
      if current_path == root:
        logging.warning('Found port at search path root: %s', current_path)
      port_name = _GetPortName(root, current_path)
      if port_name in result:
        logging.error('Found duplicate port name "%s" at "%s" and "%s"',
                      port_name, result[port_name], current_path)
      result[port_name] = current_path
      del directories[:]  # Don't detect ports inside other ports.

  return result


def _FindThirdPartyPlatforms():
  """Workhorse for GetThirdPartyPlatforms().

  Search through directories listed in _PORT_SEARCH_PATH to find valid ports, so
  that they can be added to the VALID_PLATFORMS list. This allows gyp_cobalt to
  register new ports without needing to modify code in src/cobalt/.

  Returns:
    A dictionary of name->absolute paths to the port directory.
  """

  result = {}
  for path_element in _PORT_SEARCH_PATH:
    root = os.path.join(_SOURCE_TREE_DIR, path_element)
    if not os.path.isdir(root):
      logging.warning('Port search path directory not found: %s', path_element)
      continue
    root = os.path.realpath(root)
    _FindValidPorts(root, result)

  return result


# Global cache of TPP so the filesystem walk is only done once, and the values
# are always consistent.
_THIRD_PARTY_PLATFORMS = None


def GetThirdPartyPlatforms():
  """Return valid platform definitions found by scanning the source tree."""
  global _THIRD_PARTY_PLATFORMS
  if _THIRD_PARTY_PLATFORMS is None:
    _THIRD_PARTY_PLATFORMS = _FindThirdPartyPlatforms()
  return _THIRD_PARTY_PLATFORMS
