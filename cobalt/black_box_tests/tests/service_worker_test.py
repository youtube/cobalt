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
"""Tests basic Service Worker functionality."""

import os
from six.moves import SimpleHTTPServer

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import MakeRequestHandlerClass
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

# The base path of the requested assets is the parent directory.
_SERVER_ROOT_PATH = os.path.join(os.path.dirname(__file__), os.pardir)


class ServiceWorkerRequestDetector(MakeRequestHandlerClass(_SERVER_ROOT_PATH)):
  """Proxies everything to SimpleHTTPRequestHandler, except some paths."""

  def end_headers(self):
    self.send_my_headers()
    SimpleHTTPServer.SimpleHTTPRequestHandler.end_headers(self)

  def send_header(self, header, value):
    # Ensure that the Content-Type for paths ending in '.js' are always
    # 'text/javascript'.
    if header == 'Content-Type' and self.path.endswith('.js'):
      SimpleHTTPServer.SimpleHTTPRequestHandler.send_header(
          self, header, 'text/javascript')
    else:
      SimpleHTTPServer.SimpleHTTPRequestHandler.send_header(self, header, value)

  def send_my_headers(self):
    # Add 'Service-Worker-Allowed' header for the main service worker scripts.
    if self.path.endswith(
        '/service_worker_test_worker.js') or self.path.endswith(
            '/service_worker_test_persisted_worker.js'):
      self.send_header('Service-Worker-Allowed', '/bar')

  def do_GET(self):  # pylint: disable=invalid-name
    """Handles HTTP GET requests for resources."""
    # Check if the request for the main service worker script has the
    # 'Service-Worker' header.
    if self.path.endswith(
        '/service_worker_test_worker.js') or self.path.endswith(
            '/service_worker_test_claimable.js'):
      service_worker_request_header = self.headers.get('Service-Worker', '')
      expected_service_worker_request_header = 'script'

      if not (service_worker_request_header
              == expected_service_worker_request_header):
        raise ValueError(
            'Service-Worker HTTP request header value does not '
            'match with expected value.\n'
            f'Header value:{service_worker_request_header}\n'
            f'Expected value:{expected_service_worker_request_header}')

    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


class ServiceWorkerTest(black_box_tests.BlackBoxTestCase):
  """Test basic Service Worker functionality."""

  def test_service_worker(self):

    with ThreadedWebServer(
        ServiceWorkerRequestDetector,
        binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/service_worker_test.html')

      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())
