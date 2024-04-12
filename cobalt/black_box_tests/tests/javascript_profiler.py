# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Tests if Cobalt client page can use window.Profiler."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer


class JavaScriptProfilerTest(black_box_tests.BlackBoxTestCase):
  """Ensure that the client can declare a `window.Profiler` object."""

  def test_javascript_profiler_prime_profiler(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      modes = [
          'testPrimeProfiler',
          'testSampleBufferFullProfiler',
          'testAbruptGarbageCollection',
      ]
      for mode in modes:
        url = server.GetURL(
            file_name=f'testdata/javascript_profiler.html?mode={mode}')
        with self.CreateCobaltRunner(url=url) as runner:
          runner.WaitForJSTestsSetup()
          self.assertTrue(runner.JSTestsSucceeded(),
                          f'JavaScript profiler failed at case mode="{mode}".')
