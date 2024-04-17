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
# limitations under the License
"""Test H5vcc logEvent() API"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer
from cobalt.tools.automated_testing import webdriver_utils

keys = webdriver_utils.import_selenium_module('webdriver.common.keys')


class H5vccWatchdogApiTest(black_box_tests.BlackBoxTestCase):

  def test_h5vcc_watchdog_api_test(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(
          file_name='testdata/h5vcc_watchdog_violation_test.html')
      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())

  def test_h5vcc_watchdog_restart_test(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/h5vcc_watchdog_restart_test.html')

      # The webpage listens for NUMPAD1, NUMPAD2
      # pylint: disable=broad-exception-caught
      with self.CreateCobaltRunner(url=url) as runner:
        # Press NUMPAD1 for first Cobalt start.
        runner.WaitForJSTestsSetup()
        try:
          runner.SendKeys(keys.Keys.NUMPAD1)
        except Exception as e:
          print('suppressing exception after killing Cobalt:', e)

    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/h5vcc_watchdog_restart_test.html')

      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForJSTestsSetup()
        # Press NUMPAD2 for second Cobalt start.
        runner.SendKeys(keys.Keys.NUMPAD2)
        self.assertTrue(runner.JSTestsSucceeded())

  def test_h5vcc_watchdog_restart_no_crash_test(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(
          file_name='testdata/h5vcc_watchdog_restart_no_crash_test.html')

      # The webpage listens for NUMPAD1, NUMPAD2
      # pylint: disable=broad-exception-caught
      with self.CreateCobaltRunner(url=url) as runner:
        # Press NUMPAD1 for first Cobalt start.
        runner.WaitForJSTestsSetup()
        try:
          runner.SendKeys(keys.Keys.NUMPAD1)
        except Exception as e:
          print('suppressing exception after killing Cobalt:', e)

      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForJSTestsSetup()
        # Press NUMPAD2 for second Cobalt start.
        runner.SendKeys(keys.Keys.NUMPAD2)
        self.assertTrue(runner.JSTestsSucceeded())
