# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

import _env  # pylint: disable=unused-import,g-bad-import-order

import logging
import traceback

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
    try:
      with ThreadedWebServer(
          binding_address=self.GetBindingAddress()) as server:
        url = server.GetURL(file_name='testdata/pointer_test.html')

        with self.CreateCobaltRunner(url=url) as runner:
          logging.info('JS Test Setup WaitForJSTestsSetup')
          runner.WaitForJSTestsSetup()
          logging.info('JS Test Setup')
          self.assertTrue(runner.webdriver)

          top_one = find_element_by_id(runner, 'top_one')
          top_two = find_element_by_id(runner, 'top_two')
          top_three = find_element_by_id(runner, 'top_three')
          top_four = find_element_by_id(runner, 'top_four')
          top_five = find_element_by_id(runner, 'top_five')
          top_six = find_element_by_id(runner, 'top_six')

          bottom_one = find_element_by_id(runner, 'bottom_one')
          bottom_two = find_element_by_id(runner, 'bottom_two')
          bottom_three = find_element_by_id(runner, 'bottom_three')
          bottom_four = find_element_by_id(runner, 'bottom_four')
          bottom_five = find_element_by_id(runner, 'bottom_five')
          bottom_six = find_element_by_id(runner, 'bottom_six')

          # Perform mouse actions with ActionChains.
          #   https://www.selenium.dev/selenium/docs/api/py/webdriver/selenium.webdriver.common.action_chains.html#module-selenium.webdriver.common.action_chains
          actions = ActionChains(runner.webdriver)
          actions.move_to_element(top_one).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.move_to_element(top_two).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.move_to_element_with_offset(top_two, 10,
                                              10).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.move_to_element_with_offset(top_two, 0,
                                              0).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.move_to_element_with_offset(top_two, -10,
                                              0).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.click(top_three)
          actions.click_and_hold(top_four)
          actions.release(top_five)
          actions.move_to_element(top_six).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.move_to_element(bottom_six).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.click(bottom_five)
          actions.click_and_hold(bottom_four)
          actions.release(bottom_three)
          actions.move_to_element(bottom_two).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.move_to_element(bottom_one).pause(_SLEEP_AFTER_MOVE_TIME)
          actions.perform()

          find_element_by_id(runner, 'end').click()
          self.assertTrue(runner.JSTestsSucceeded())
    except:  # pylint: disable=bare-except
      traceback.print_exc()
      # Consider an exception being thrown as a test failure.
      self.assertTrue(False)
    finally:
      logging.info('Cleaning up.')
