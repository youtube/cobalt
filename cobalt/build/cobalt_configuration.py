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

import os

import _env  # pylint: disable=unused-import
from cobalt.tools import paths
import cobalt.tools.webdriver_benchmark_config as wb_config
from starboard.build import application_configuration
from starboard.tools.config import Config

# The canonical Cobalt application name.
APPLICATION_NAME = 'cobalt'


class CobaltConfiguration(application_configuration.ApplicationConfiguration):
  """Base Cobalt configuration class.

  Cobalt per-platform configurations, if defined, must subclass from this class.
  """

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltConfiguration,
          self).__init__(platform_configuration, application_name,
                         application_directory)

  def GetVariables(self, config_name):

    # Use env var to optimize build speed on CI
    try:
      # Force to int, so it's easy to pass in an override.
      use_fastbuild = int(os.environ.get('IS_CI', 0))
    except (ValueError, TypeError):
      use_fastbuild = 0

    try:
      build_in_docker = int(os.environ.get('IS_DOCKER', 0))
    except (ValueError, TypeError):
      build_in_docker = 0

    variables = {
        # This is used to omit large debuginfo in files on CI environment
        'cobalt_fastbuild': use_fastbuild,
        'cobalt_docker_build': build_in_docker,

        # This is here rather than cobalt_configuration.gypi so that it's
        # available for browser_bindings_gen.gyp.
        'enable_debugger': 0 if config_name == Config.GOLD else 1,

        # Cobalt uses OpenSSL on all platforms.
        'use_openssl': 1,
    }
    return variables

  def GetPostIncludes(self):
    # Insert cobalt_configuration.gypi into the post includes list.
    includes = super(CobaltConfiguration, self).GetPostIncludes()
    includes[:0] = [os.path.join(paths.BUILD_ROOT, 'cobalt_configuration.gypi')]
    return includes

  def GetWebPlatformTestFilters(self):
    """Gets all tests to be excluded from a black box test run."""

    # Skipped tests on all platforms due to HTTP proxy bugs.
    # Tests pass with a direct SSH tunnel.
    # Proxy sends out response lines as soon as it gets it without waiting
    # for the entire response. It is possible that this causes issues or that
    # the proxy has problems sending and terminating a single complete
    # response. It may end up sending multiple empty responses.
    filters = [
        # Late listeners: Preflight.
        # Disabled because of: Flaky. Buildbot only failure.
        'cors/WebPlatformTest.Run/cors_late_upload_events_htm',

        # getResponseHeader: Combined testing of cors response headers.
        # Disabled because of: Timeout.
        'cors/WebPlatformTest.Run/cors_response_headers_htm',

        # Status on GET 400, HEAD 401, POST 404, PUT 200.
        # Disabled because of: Response status returning 0.
        'cors/WebPlatformTest.Run/cors_status_async_htm',

        # CORS - status after preflight on POST 401, POST 404, PUT 699.
        # Disabled because of: Response status returning 0 or timeout.
        'cors/WebPlatformTest.Run/cors_status_preflight_htm',

        # Response reader closed promise should reject after a network error
        # happening after resolving fetch promise.
        # Disabled because of: Timeout.
        'fetch/WebPlatformTest.Run/fetch_api_basic_error_after_response_html',

        # RequestCache "no-store" mode does not store the response in the
        # cache with Last-Modified and fresh response.
        # Disabled because of: Timeout. Buildbot only failure.
        ('fetch/WebPlatformTest.Run/'
         'fetch_api_request_request_cache_no_store_html'),

        # Check response clone use structureClone for teed ReadableStreams
        # (DataViewchunk).
        # Disabled because of: Timeout.
        'fetch/WebPlatformTest.Run/fetch_api_response_response_clone_html',

        # XMLHttpRequest: send() - Basic authenticated CORS request using
        # setRequestHeader().
        # Disabled because of: Timeout. Buildbot only failure.
        ('xhr/WebPlatformTest.Run/'
         'XMLHttpRequest_cobalt_trunk_send_authentication_cors_basic_'
         'setrequestheader_htm'),

        # XMLHttpRequest: send() - CORS request with setRequestHeader auth to
        # URL accepting Authorization header.
        # Disabled because of: False user and password. Buildbot only failure.
        ('xhr/WebPlatformTest.Run/'
         'XMLHttpRequest_cobalt_trunk_send_authentication_cors_'
         'setrequestheader_no_cred_htm'),

        # XMLHttpRequest: send() - Redirects (basics) (307).
        # Disabled because of: Flaky.
        'xhr/WebPlatformTest.Run/XMLHttpRequest_send_redirect_htm',

        # Disabled because of: Flaky on buildbot across multiple buildconfigs.
        # Non-reproducible with local runs.
        ('xhr/WebPlatformTest.Run/'
         'XMLHttpRequest_send_entity_body_get_head_async_htm'),
        'xhr/WebPlatformTest.Run/XMLHttpRequest_status_error_htm',
        'xhr/WebPlatformTest.Run/XMLHttpRequest_response_json_htm',
        'xhr/WebPlatformTest.Run/XMLHttpRequest_send_redirect_to_non_cors_htm',
    ]
    return filters

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
        'extension_test',
        'graphics_system_test',
        'layout_test',
        'layout_tests',
        'cwrappers_test',
        'loader_test',
        'math_test',
        'media_capture_test',
        'media_session_test',
        'media_stream_test',
        'memory_store_test',
        'nb_test',
        'net_unittests',
        'network_test',
        'poem_unittests',
        'render_tree_test',
        'renderer_test',
        'sql_unittests',
        'storage_test',
        'storage_upgrade_test',
        'text_encoding_test',
        'web_animations_test',
        'webdriver_test',
        'websocket_test',
        'xhr_test',
        'zip_unittests',
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
