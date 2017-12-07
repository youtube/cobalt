# Copyright 2017 Google Inc. All Rights Reserved.
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

import os

import _env  # pylint: disable=unused-import
from cobalt.build import gyp_utils
from cobalt.tools import paths
import cobalt.tools.webdriver_benchmark_config as wb_config
from starboard.build import application_configuration


# The canonical Cobalt application name.
APPLICATION_NAME = 'cobalt'


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
        'cobalt_version': gyp_utils.GetBuildNumber(),

        # Cobalt uses OpenSSL on all platforms.
        'use_openssl': 1,

        # Set environmental variable to enable_vr: 'USE_VR'
        # Terminal: `export {varname}={value}`
        # Note: must also edit gyp_configuration.gypi per internal instructions.

        # Whether to enable VR.
        'enable_vr': int(os.environ.get('USE_VR', 0)),
    }
    return variables

  def GetPostIncludes(self):
    # Insert cobalt_configuration.gypi into the post includes list.
    includes = super(CobaltConfiguration, self).GetPostIncludes()
    includes[:0] = [os.path.join(paths.BUILD_ROOT, 'cobalt_configuration.gypi')]
    return includes

  def GetTestTargets(self):
    return [
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
        'nb_test',
        'net_unittests',
        'network_test',
        'page_visibility_test',
        'poem_unittests',
        'render_tree_test',
        'renderer_test',
        'sql_unittests',
        'storage_test',
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
