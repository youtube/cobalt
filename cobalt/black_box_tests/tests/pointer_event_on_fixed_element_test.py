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
"""Tests events for an extended sequence of pointer moves and clicks."""

# This test generates an extended sequence of pointer move and click events,
# and verifies the corresponding sequence of pointer, mouse, and click events
# dispatched to the HTML elements. The test includes use of pointer capture and
# use of preventDefault() and stopPropagation() on the events, as well as event
# bubbling.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import logging

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer
from selenium.webdriver.common.action_chains import ActionChains

# Time to sleep after a mouse move to give the device time to process it and
# to avoid collapsing with subsequent move events.
_SLEEP_AFTER_MOVE_TIME = 0.5


def find_element_by_id(runner, id_selector):
  return runner.webdriver.find_elements_by_css_selector('#' + id_selector)[0]


class PointerTest(black_box_tests.BlackBoxTestCase):
  """Tests pointer and mouse event."""

  def test_pointer_events(self):
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(
          file_name='testdata/pointer_event_on_fixed_element_test.html')

      with self.CreateCobaltRunner(url=url) as runner:
        logging.info('JS Test Setup WaitForJSTestsSetup')
        runner.WaitForJSTestsSetup()
        logging.info('JS Test Setup')
        self.assertTrue(runner.webdriver)

        container = find_element_by_id(runner, 'container')
        surface = find_element_by_id(runner, 'surface')

        actions = ActionChains(runner.webdriver)
        actions.move_to_element(container).pause(_SLEEP_AFTER_MOVE_TIME)
        actions.click(container).pause(_SLEEP_AFTER_MOVE_TIME)
        actions.move_to_element(surface).pause(_SLEEP_AFTER_MOVE_TIME)
        actions.click(surface).pause(_SLEEP_AFTER_MOVE_TIME)
        actions.perform()
        self.assertTrue(runner.JSTestsSucceeded())
