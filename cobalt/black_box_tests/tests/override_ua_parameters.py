# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Test overriding UA parameters when launching Cobalt."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import SimpleHTTPServer

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import MakeRequestHandlerClass
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

# The base path of the requested assets is the parent directory.
_SERVER_ROOT_PATH = os.path.join(os.path.dirname(__file__), os.pardir)


class JavascriptRequestDetector(MakeRequestHandlerClass(_SERVER_ROOT_PATH)):
  """Proxies everything to SimpleHTTPRequestHandler, except some paths."""

  def do_GET(self):  # pylint: disable=invalid-name
    """Handles HTTP GET requests for resources."""
    # Check if UA string in the request header reflects correct UA params
    # overrides
    ua_request_header = self.headers.get('user-agent', '')
    expected_ua_request_header = 'Mozilla/5.0 (Corge grault-v7a; '\
        'Garply 7.1.2; Waldo OS 6.0) Cobalt/21.lts.2.289852-debug '\
        '(unlike Gecko) v8/7.7.299.8-jit gles Starboard/12, '\
        'Quuz_ATV_foobar0000_2018/Unknown (Cobalt, QUUX, Wireless) '\
        'foo.bar.baz.qux/21.2.1.41.0'

    if not ua_request_header == expected_ua_request_header:
      raise ValueError('UA string in HTTP request header does not match with '\
          'UA params overrides specified in command line\n'\
          'UA string in HTTP request header:%s' % (ua_request_header))

    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


class OverrideUAParametersTest(black_box_tests.BlackBoxTestCase):
  """Test overriding UA parameters."""

  def test_simple(self):
    """Set UA parameters when launching Cobalt."""

    with ThreadedWebServer(
        JavascriptRequestDetector,
        binding_address=self.GetBindingAddress()) as server:
      with self.CreateCobaltRunner(
          url=server.GetURL(file_name='testdata/override_ua_parameters.html'),
          target_params=[
              '--user_agent_client_hints='\
              'aux_field=foo.bar.baz.qux/21.2.1.41.0;'\
              'brand=Cobalt;'\
              'build_configuration=debug;'\
              'chipset_model_number=foobar0000;'\
              'cobalt_build_version_number=289852;'\
              'cobalt_version=21.lts.2;'\
              'connection_type=Wireless;'\
              'device_type=ATV;'\
              'evergreen_type=;'\
              'evergreen_version=;'\
              'javascript_engine_version=v8/7.7.299.8-jit;'\
              'firmware_version=;'\
              'model=QUUX;'\
              'model_year=2018;'\
              'original_design_manufacturer=Quuz;'\
              'os_name_and_version=Corge grault-v7a\\; Garply 7.1.2\\; '\
                  'Waldo OS 6.0;'\
              'starboard_version=Starboard/12;'\
              'rasterizer_type=gles'
          ]) as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())
