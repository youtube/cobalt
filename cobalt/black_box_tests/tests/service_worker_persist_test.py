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
"""Tests Service Worker Persistence functionality."""

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer
from cobalt.tools.automated_testing import webdriver_utils
from cobalt.black_box_tests.tests.service_worker_test import ServiceWorkerRequestDetector

keys = webdriver_utils.import_selenium_module('webdriver.common.keys')


class ServiceWorkerPersistTest(black_box_tests.BlackBoxTestCase):
  """Test basic Service Worker functionality."""

  def test_service_worker_persist(self):

    with ThreadedWebServer(
        ServiceWorkerRequestDetector,
        binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/service_worker_persist_test.html')

      # NUMPAD0 calls test_successful_registration()
      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForActiveElement()
        runner.SendKeys(keys.Keys.NUMPAD0)
        self.assertTrue(runner.JSTestsSucceeded())
      # NUMPAD1 calls test_persistent_registration()
      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForActiveElement()
        runner.SendKeys(keys.Keys.NUMPAD1)
        self.assertTrue(runner.JSTestsSucceeded())
      # NUMPAD2 calls test_persistent_registration_does_not_exist()
      with self.CreateCobaltRunner(url=url) as runner:
        runner.WaitForActiveElement()
        runner.SendKeys(keys.Keys.NUMPAD2)
        self.assertTrue(runner.JSTestsSucceeded())
