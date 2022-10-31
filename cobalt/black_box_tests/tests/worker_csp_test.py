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
"""Tests workers handling CSP Violations."""

import os

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer, MakeCustomHeaderRequestHandlerClass

paths_to_headers = {
    'worker_load_csp_test.html': {
        'Content-Security-Policy':
            "script-src 'unsafe-inline' 'self'; worker-src 'self'"
    },
    'worker_csp_test.html': {
        'Content-Security-Policy':
            "script-src 'unsafe-inline' 'self'; connect-src 'self'"
    },
    'worker_csp_test.js': {
        'Content-Security-Policy': "connect-src 'self';"
    }
}


class WorkerCspTest(black_box_tests.BlackBoxTestCase):
  """Verify correct correct CSP behavior."""

  def test_1_worker_csp(self):
    path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    with ThreadedWebServer(
        binding_address=self.GetBindingAddress(),
        handler=MakeCustomHeaderRequestHandlerClass(
            path, paths_to_headers)) as server:
      url = server.GetURL(file_name='testdata/worker_csp_test.html')

      with self.CreateCobaltRunner(url=url) as runner:
        self.assertTrue(runner.JSTestsSucceeded())

  def test_2_worker_load_csp(self):
    path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    with ThreadedWebServer(
        binding_address=self.GetBindingAddress(),
        handler=MakeCustomHeaderRequestHandlerClass(
            path, paths_to_headers)) as server:
      url = server.GetURL(file_name='testdata/worker_load_csp_test.html')

      with self.CreateCobaltRunner(url=url) as runner:
        self.assertTrue(runner.JSTestsSucceeded())

  def test_3_service_worker_csp(self):
    path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    with ThreadedWebServer(
        binding_address=self.GetBindingAddress(),
        handler=MakeCustomHeaderRequestHandlerClass(
            path, paths_to_headers)) as server:
      url = server.GetURL(file_name='testdata/service_worker_csp_test.html')

      with self.CreateCobaltRunner(url=url) as runner:
        self.assertTrue(runner.JSTestsSucceeded())
