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
"""Tests if Cobalt properly caches resources that were previously loaded."""

import os
from six.moves import SimpleHTTPServer
from six.moves.urllib.parse import urlparse
import time

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import MakeRequestHandlerClass
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

# The base path of the requested assets is the parent directory.
_SERVER_ROOT_PATH = os.path.join(os.path.dirname(__file__), os.pardir)


class DelayedHttpRequestHandler(MakeRequestHandlerClass(_SERVER_ROOT_PATH)):
  """Handles HTTP requests but adds a delay before serving a response."""

  def do_GET(self):  # pylint: disable=invalid-name
    """Handles HTTP GET requests for resources."""

    parsed_path = urlparse(self.path)

    if parsed_path.path.startswith('/testdata/http_cache_test_resources/'):
      time.sleep(3)

    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


class HttpCacheTest(black_box_tests.BlackBoxTestCase):
  """Load resources, then reload the page and verify."""

  def test_http_cache(self):

    with ThreadedWebServer(
        binding_address=self.GetBindingAddress(),
        handler=DelayedHttpRequestHandler) as server:
      url = server.GetURL(file_name='testdata/http_cache.html')
      with self.CreateCobaltRunner(url=url) as runner:
        self.assertTrue(runner.JSTestsSucceeded())
