# Copyright 2012 The Cobalt Authors. All Rights Reserved.
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

_SUBREPO_PATHS = ['starboard/keyboxes']
_VERSION_SERVER_URL = 'https://carbon-airlock-95823.appspot.com/build_version/generate'  # pylint:disable=line-too-long
_XSSI_PREFIX = ")]}'\n"

# The path to the build.id file that preserves a build ID.
BUILD_ID_PATH = os.path.join(paths.BUILD_ROOT, 'build.id')


def CheckRevInfo(key, cwd=None):
  cwd = cwd if cwd else '.'
  git_prefix = ['git', '-C', cwd]
  git_get_remote_args = git_prefix + ['config', '--get', 'remote.origin.url']
  remote = subprocess.check_output(git_get_remote_args).strip()

  if remote.endswith('.git'):
    remote = remote[:-len('.git')]

  git_get_revision_args = git_prefix + ['rev-parse', 'HEAD']
  revision = subprocess.check_output(git_get_revision_args).strip()
  return {key: '{}@{}'.format(remote, revision)}


def GetRevinfo():
  """Get absolute state of all git repos."""
  try:
    repo_root = subprocess.check_output(['git', 'rev-parse',
                                         '--show-toplevel']).strip()
  except subprocess.CalledProcessError as e:
    logging.info('Could not get repo root. Trying again in src/')
    try:
      repo_root = subprocess.check_output(
          ['git', '-C', 'src', 'rev-parse', '--show-toplevel']).strip()
    except subprocess.CalledProcessError as e:
      logging.warning('Failed to get revision information: %s', e)
      return {}

  # First make sure we can add the cobalt_src repo.
  try:
    repos = CheckRevInfo('.', cwd=repo_root)
  except subprocess.CalledProcessError as e:
    logging.warning('Failed to get revision information: %s', e)
    return {}

  for rel_path in _SUBREPO_PATHS:
    path = os.path.join(repo_root, rel_path)
    try:
      repos.update(CheckRevInfo(rel_path, cwd=path))
    except subprocess.CalledProcessError as e:
      logging.warning('Failed to get revision information for subrepo: %s', e)
      continue

  return repos


def GetBuildNumber(version_server=_VERSION_SERVER_URL):
  """Send a request to the build version server for a build number."""

  if os.path.isfile(BUILD_ID_PATH):
    with open(BUILD_ID_PATH, 'r') as build_id_file:
      build_number = int(build_id_file.read().replace('\n', ''))
      logging.info('Retrieving build number from %s', BUILD_ID_PATH)
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
    logging.critical(
        'Could not query constant value.  The expression '
        'should only have numbers, operators, spaces, and '
        'parens.  Please check "%s" in %s.\n', constant_name, file_path)
    sys.exit(1)

  expression = match.group(1)
  value = eval(expression)  # pylint:disable=eval-used
  return value
