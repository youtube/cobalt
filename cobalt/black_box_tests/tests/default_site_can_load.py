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
"""Tests that Cobalt can load the default site."""

import logging
import traceback
import time

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer


class DefaultSiteCanLoadTest(black_box_tests.BlackBoxTestCase):
  """Let Cobalt load the default site and perform some basic sanity checks."""

  def test_default_site_can_load(self):
    try:
      with ThreadedWebServer(binding_address=self.GetBindingAddress()):
        # Note: This test will fail when the Internet can not be reached from
        # the device running Cobalt.

        with self.CreateCobaltRunner() as runner:
          # Allow for app startup time.
          logging.info('Wait to allow app startup.')
          time.sleep(10)
          logging.info('Wait for html element.')
          # Wait until there is at least the root element.
          runner.PollUntilFound('html')
          # Check that certain elements that should always be there are there,
          # and remain there for a few seconds.
          for i in range(10):
            time.sleep(1)
            logging.info(
                ('Checking for consistent html, head, body, script, and div'
                 ' elements: %d'), i)
            # Expect at least one <html> (root) element in the page
            self.assertTrue(runner.FindElements('html'))
            # Expect at least one <head> element in the page
            self.assertTrue(runner.FindElements('head'))
            # Expect at least one <body> element in the page
            self.assertTrue(runner.FindElements('body'))
            # Expect at least one <script> element in the page
            self.assertTrue(runner.FindElements('script'))
            # Expect at least three <div> elements in the page
            self.assertTrue(len(runner.FindElements('div')) > 2)
    except:  # pylint: disable=bare-except
      traceback.print_exc()
      # Consider an exception being thrown as a test failure.
      self.fail('Test failure')
    finally:
      logging.info('Cleaning up.')
