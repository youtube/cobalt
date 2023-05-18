# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Tests successful update to Cobalt binary available on the test channel."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer


class EvergreenVerifyQaChannelUpdateTest(black_box_tests.BlackBoxTestCase):

  def test_evergreen_verify_qa_channel_update(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(
          file_name='testdata/evergreen_test.html?resetInstallations=true')
      # Resetting the installations doesn't require an update check.
      with self.CreateCobaltRunner(
          url=url,
          target_params=['--update_check_delay_seconds=300'],
          loader_target='loader_app') as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())

      url = server.GetURL(
          file_name='testdata/evergreen_test.html?channel=test&status=Update installed, pending restart'  # pylint: disable=line-too-long
          .replace(' ', '%20'))
      # 100 seconds provides enough time for the initial update delay, the prod
      # channel update, and the target channel update.
      with self.CreateCobaltRunner(
          url=url, poll_until_wait_seconds=100,
          loader_target='loader_app') as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())

      url = server.GetURL(
          file_name='testdata/evergreen_test.html?channel=test&status=App is up to date'  # pylint: disable=line-too-long
          .replace(' ', '%20'))
      # 60 seconds provides enough time for the initial update delay and target
      # channel update check.
      with self.CreateCobaltRunner(
          url=url, poll_until_wait_seconds=60,
          loader_target='loader_app') as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())

      url = server.GetURL(
          file_name='testdata/evergreen_test.html?resetInstallations=true')
      # Resetting the installations doesn't require an update check.
      with self.CreateCobaltRunner(
          url=url,
          target_params=['--update_check_delay_seconds=300'],
          loader_target='loader_app') as runner:
        runner.WaitForJSTestsSetup()
        self.assertTrue(runner.JSTestsSucceeded())
