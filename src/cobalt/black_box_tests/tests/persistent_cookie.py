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

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer
from cobalt.tools.automated_testing import webdriver_utils

keys = webdriver_utils.import_selenium_module('webdriver.common.keys')


class PersistentCookieTest(black_box_tests.BlackBoxTestCase):
  """Ensure that Cobalt can have persistent cookie across sessions/launches."""

  # The same page has to be used since cookie are stored per URL.

  def test_simple(self):

    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/persistent_cookie.html')

      # The webpage listens for NUMPAD1, NUMPAD2 and NUMPAD3 at opening.
      with self.CreateCobaltRunner(url=url) as runner:
        # Press NUMPAD1 to verify basic cookie functionality and set
        # a persistent cookie.
        runner.WaitForJSTestsSetup()
        runner.SendKeys(keys.Keys.NUMPAD1)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForJSTestsSetup()
        # Press NUMPAD2 to indicate this is the second time we opened
        # the webpage and verify a persistent cookie is on device. Then
        # clear this persistent cookie.
        runner.SendKeys(keys.Keys.NUMPAD2)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForJSTestsSetup()
        # Press NUMPAD3 to verify the persistent cookie we cleared is
        # not on the device for this URL any more.
        runner.SendKeys(keys.Keys.NUMPAD3)
        self.assertTrue(runner.JSTestsSucceeded())
