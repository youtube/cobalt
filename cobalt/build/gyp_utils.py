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

import json
import logging
import os
import re
import subprocess
import sys
import urllib
import urllib2

import _env  # pylint: disable=unused-import
from cobalt.tools import paths
from starboard.build import clang
from starboard.tools import build


_VERSION_SERVER_URL = 'https://carbon-airlock-95823.appspot.com/build_version/generate'  # pylint:disable=line-too-long
_XSSI_PREFIX = ")]}'\n"

# The path to the build.id file that preserves a build ID.
BUILD_ID_PATH = os.path.join(paths.BUILD_ROOT, 'build.id')


def GetRevinfo():
  """Get absolute state of all git repos from gclient DEPS."""

  try:
    revinfo_cmd = ['gclient', 'revinfo', '-a']

    if sys.platform.startswith('linux') or sys.platform == 'darwin':
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
  except (subprocess.CalledProcessError, ValueError) as e:
    logging.warning('Failed to get revision information: %s', e)
    try:
      logging.warning('Command output was: %s', line)
    except NameError:
      pass
    return {}


def GetBuildNumber(version_server=_VERSION_SERVER_URL):
  """Send a request to the build version server for a build number."""

  if os.path.isfile(BUILD_ID_PATH):
    with open(BUILD_ID_PATH, 'r') as build_id_file:
      build_number = int(build_id_file.read().replace('\n', ''))
      logging.info('Retrieving build number from %s', BUILD_ID_PATH)
      logging.info('Build Number: %d', build_number)
      return build_number

  revinfo = GetRevinfo()
  json_deps = json.dumps(revinfo)
  username = os.environ.get('USERNAME', os.environ.get('USER'))

  post_data = {'deps': json_deps}
  if username:
    post_data['user'] = username

  logging.debug('Post data is %s', post_data)
  request = urllib2.Request(version_server, data=urllib.urlencode(post_data))
  # TODO: retry on timeout.
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
                     'parens.  Please check "%s" in %s.\n', constant_name,
                     file_path)
    sys.exit(1)

  expression = match.group(1)
  value = eval(expression)  # pylint:disable=eval-used
  return value


def GetHostCompilerEnvironment(goma_supports_compiler=False):
  # Assume the same clang that Starboard declares.
  return build.GetHostCompilerEnvironment(clang.GetClangSpecification(),
                                          goma_supports_compiler)
