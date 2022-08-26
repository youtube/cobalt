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
import subprocess
from six.moves import urllib
from cobalt.tools import paths

_SUBREPO_PATHS = ['starboard/keyboxes']
_VERSION_SERVER_URL = 'https://carbon-airlock-95823.appspot.com/build_version/generate'  # pylint:disable=line-too-long
_XSSI_PREFIX = ")]}'\n"

# The path to the build.id file that preserves a build ID.
BUILD_ID_PATH = os.path.join(paths.BUILD_ROOT, 'build.id')


def CheckRevInfo(key, cwd=None):
  git_get_remote_args = ['git', 'config', '--get', 'remote.origin.url']
  remote = subprocess.check_output(
      git_get_remote_args, cwd=cwd).strip().decode('utf-8')

  if remote.endswith('.git'):
    remote = remote[:-len('.git')]

  git_get_revision_args = ['git', 'rev-parse', 'HEAD']
  revision = subprocess.check_output(
      git_get_revision_args, cwd=cwd).strip().decode('utf-8')
  return {key: '{}@{}'.format(remote, revision)}


def GetRevinfo():
  """Get absolute state of all git repos."""
  # First make sure we can add the cobalt_src repo.
  try:
    repos = CheckRevInfo('.', cwd=paths.REPOSITORY_ROOT)
  except subprocess.CalledProcessError as e:
    logging.warning('Failed to get revision information: %s', e)
    return {}

  for rel_path in _SUBREPO_PATHS:
    path = os.path.join(paths.REPOSITORY_ROOT, rel_path)
    try:
      repos.update(CheckRevInfo(rel_path, cwd=path))
    except subprocess.CalledProcessError as e:
      logging.warning('Failed to get revision information for subrepo %s: %s',
                      rel_path, e)
      continue
    except OSError as e:
      logging.info('%s. Subrepository %s not found.', e, rel_path)
      continue

  return repos


# We leave this function in for backwards compatibility.
# New callers should use GetOrGenerateNewBuildNumber.
def GetBuildNumber(version_server=_VERSION_SERVER_URL):
  return GetOrGenerateNewBuildNumber(version_server)


def GetOrGenerateNewBuildNumber(version_server=_VERSION_SERVER_URL):
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
  request = urllib.request.Request(version_server)
  # TODO: retry on timeout.
  try:
    response = urllib.request.urlopen(  # pylint: disable=consider-using-with
        request,
        data=urllib.parse.urlencode(post_data).encode('utf-8'))
    data = response.read().decode('utf-8')
    if data.find(_XSSI_PREFIX) == 0:
      data = data[len(_XSSI_PREFIX):]
    results = json.loads(data)
    build_number = results.get('build_number', 0)
    return build_number
  except urllib.error.HTTPError as e:
    logging.warning('Failed to retrieve build number: %s', e)
    return 0
  except urllib.error.URLError as e:
    logging.warning('Could not connect to %s: %s', version_server, e)
    return 0
