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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import time

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

_MAX_RESUME_WAIT_SECONDS = 30


class PreloadFontTest(black_box_tests.BlackBoxTestCase):
  """Verify that fonts are loaded correctly after preload event."""

  def test_simple(self):

    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/preload_font.html')

      with self.CreateCobaltRunner(
          url=url, target_params=['--preload']) as runner:
        runner.WaitForJSTestsSetup()
        runner.SendResume()
        start_time = time.time()
        while runner.IsInPreload():
          if time.time() - start_time > _MAX_RESUME_WAIT_SECONDS:
            raise Exception('Cobalt can not exit preload mode after receiving'
                            'resume signal')
          time.sleep(.1)
        # At this point, Cobalt is in started mode.
        self.assertTrue(runner.JSTestsSucceeded())
