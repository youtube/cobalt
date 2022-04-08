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
# limitations under the License
"""Test SoftMicPlatformService messages match between web app and platform"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer
from cobalt.tools.automated_testing import webdriver_utils

keys = webdriver_utils.import_selenium_module('webdriver.common.keys')


class SoftMicPlatformServiceTest(black_box_tests.BlackBoxTestCase):

  def test_soft_mic_platform_service(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(
          file_name='testdata/soft_mic_platform_service_test.html')

      # The webpage listens for NUMPAD0 through NUMPAD9 at opening
      # to test hasHardMicSupport and hasSoftMicSupport switch values.
      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD0 to test testIncorrectRequests
        runner.SendKeys(keys.Keys.NUMPAD0)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD1 to test bothUndefined
        runner.SendKeys(keys.Keys.NUMPAD1)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--has_soft_mic_support=true']) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD2 to test hardMicUndefinedSoftMicTrue
        runner.SendKeys(keys.Keys.NUMPAD2)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--has_soft_mic_support=false']) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD3 to test hardMicUndefinedSoftMicFalse
        runner.SendKeys(keys.Keys.NUMPAD3)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--has_hard_mic_support=true']) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD4 to test hardMicTrueSoftMicUndefined
        runner.SendKeys(keys.Keys.NUMPAD4)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url,
          target_params=([
              '--has_hard_mic_support=true', '--has_soft_mic_support=true'
          ])) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD5 to test hardMicTrueSoftMicTrue
        runner.SendKeys(keys.Keys.NUMPAD5)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url,
          target_params=([
              '--has_hard_mic_support=true', '--has_soft_mic_support=false'
          ])) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD6 to test hardMicTrueSoftMicFalse
        runner.SendKeys(keys.Keys.NUMPAD6)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--has_hard_mic_support=false']) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD7 to test hardMicFalseSoftMicUndefined
        runner.SendKeys(keys.Keys.NUMPAD7)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url,
          target_params=([
              '--has_hard_mic_support=false', '--has_soft_mic_support=true'
          ])) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD8 to test hardMicFalseSoftMicTrue
        runner.SendKeys(keys.Keys.NUMPAD8)
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url,
          target_params=([
              '--has_hard_mic_support=false', '--has_soft_mic_support=false'
          ])) as runner:
        runner.WaitForActiveElement()
        # Press NUMPAD9 to test hardMicFalseSoftMicFalse
        runner.SendKeys(keys.Keys.NUMPAD9)
        self.assertTrue(runner.JSTestsSucceeded())

      # The webpage listens for NUMPAD0 through NUMPAD9 at opening with SHIFT
      # to test micGesture tap and hold switch values.
      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForActiveElement()
        # Press SHIFT, NUMPAD0 to test micGestureNull
        runner.SendKeys([keys.Keys.SHIFT, keys.Keys.NUMPAD0])
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--mic_gesture=foo']) as runner:
        runner.WaitForActiveElement()
        # Press SHIFT, NUMPAD0 to test micGestureNull
        runner.SendKeys([keys.Keys.SHIFT, keys.Keys.NUMPAD0])
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--mic_gesture=hold']) as runner:
        runner.WaitForActiveElement()
        # Press SHIFT, NUMPAD1 to test micGestureHold
        runner.SendKeys([keys.Keys.SHIFT, keys.Keys.NUMPAD1])
        self.assertTrue(runner.JSTestsSucceeded())

      with self.CreateCobaltRunner(
          url=url, target_params=['--mic_gesture=tap']) as runner:
        runner.WaitForActiveElement()
        # Press SHIFT, NUMPAD2 to test micGestureTap
        runner.SendKeys([keys.Keys.SHIFT, keys.Keys.NUMPAD2])
        self.assertTrue(runner.JSTestsSucceeded())
