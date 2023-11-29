# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Base cobalt configuration for GYP."""

import logging
import os
import re
import subprocess

import _env  # pylint: disable=unused-import
from cobalt.build import gyp_utils
from cobalt.tools import paths
import cobalt.tools.webdriver_benchmark_config as wb_config
from starboard.build import application_configuration

# The canonical Cobalt application name.
APPLICATION_NAME = 'cobalt'

COMMIT_COUNT_BUILD_NUMBER_OFFSET = 1000000

# Matches numbers > 1000000. The pattern is basic so git log --grep is able to
# interpret it.
GIT_BUILD_NUMBER_PATTERN = r'[1-9]' + r'[0-9]' * 6 + r'[0-9]*'
BUILD_NUMBER_TAG_PATTERN = r'^(Build-Id: |BUILD_NUMBER=){}$'

# git log --grep can't handle capture groups.
BUILD_NUBER_PATTERN_WITH_CAPTURE = '({})'.format(GIT_BUILD_NUMBER_PATTERN)


def GetBuildNumberFromCommits():
  full_pattern = BUILD_NUMBER_TAG_PATTERN.format(GIT_BUILD_NUMBER_PATTERN)
  output = subprocess.check_output(
      ['git', 'log', '--grep', full_pattern, '-1', '--pretty=%b']).decode()

  full_pattern_with_capture = re.compile(
      BUILD_NUMBER_TAG_PATTERN.format(BUILD_NUBER_PATTERN_WITH_CAPTURE),
      flags=re.MULTILINE)
  match = full_pattern_with_capture.search(output)
  return match.group(2) if match else None


def GetBuildNumberFromServer():
  # Note $BUILD_ID_SERVER_URL will always be set in CI.
  build_id_server_url = os.environ.get('BUILD_ID_SERVER_URL')
  if not build_id_server_url:
    return None

  build_num = gyp_utils.GetBuildNumber(version_server=build_id_server_url)
  if build_num == 0:
    raise ValueError('The build number received was zero.')
  return build_num


def GetBuildNumberFromCommitCount():
  output = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'])
  build_number = int(output.strip().decode('utf-8'))
  return build_number + COMMIT_COUNT_BUILD_NUMBER_OFFSET


def GetBuildNumber():
  build_number = GetBuildNumberFromCommits()

  if not build_number:
    build_number = GetBuildNumberFromServer()

  if not build_number:
    build_number = GetBuildNumberFromCommitCount()

  return build_number


class CobaltConfiguration(application_configuration.ApplicationConfiguration):
  """Base Cobalt configuration class.

  Cobalt per-platform configurations, if defined, must subclass from this class.
  """

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltConfiguration, self).__init__(
        platform_configuration, application_name, application_directory)

  def GetVariables(self, config_name):
    variables = {
        'cobalt_fastbuild': os.environ.get('LB_FASTBUILD', 0),
        'cobalt_version': GetBuildNumber(),

        # Cobalt uses OpenSSL on all platforms.
        'use_openssl': 1,
    }
    logging.info('Build Number: {}'.format(variables['cobalt_version']))
    return variables

  def GetPostIncludes(self):
    # Insert cobalt_configuration.gypi into the post includes list.
    includes = super(CobaltConfiguration, self).GetPostIncludes()
    includes[:0] = [os.path.join(paths.BUILD_ROOT, 'cobalt_configuration.gypi')]
    return includes

  def GetTestTargets(self):
    return [
        'accessibility_test',
        'audio_test',
        'base_test',
        'base_unittests',
        'bindings_test',
        'browser_test',
        'crypto_unittests',
        'csp_test',
        'css_parser_test',
        'cssom_test',
        'dom_parser_test',
        'dom_test',
        'layout_test',
        'layout_tests',
        'loader_test',
        'math_test',
        'media_session_test',
        'media_stream_test',
        'memory_store_test',
        'nb_test',
        'net_unittests',
        'network_test',
        'page_visibility_test',
        'poem_unittests',
        'render_tree_test',
        'renderer_test',
        'sql_unittests',
        'storage_test',
        'storage_upgrade_test',
        'trace_event_test',
        'web_animations_test',
        'web_platform_tests',
        'webdriver_test',
        'xhr_test',
    ]

  def GetDefaultTargetBuildFile(self):
    return os.path.join(paths.BUILD_ROOT, 'all.gyp')

  def WebdriverBenchmarksEnabled(self):
    """Determines if webdriver benchmarks are enabled or not.

    Returns:
      True if webdriver benchmarks can run on this platform, False if not.
    """
    return False

  def GetDefaultSampleSize(self):
    return wb_config.STANDARD_SIZE

  def GetWebdriverBenchmarksTargetParams(self):
    """Gets command line params to pass to the Cobalt executable."""
    return []

  def GetWebdriverBenchmarksParams(self):
    """Gets command line params to pass to the webdriver benchmark script."""
    return []
