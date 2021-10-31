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
"""Tests sending deep links."""

# This test script works by splitting the work over 3 threads, so that they
# can each make progress even if they come across blocking operations.
# The three threads are:
#   1. Main thread, runs BlackBoxTestCase, sends suspend/resume signals, etc.
#   2. HTTP Server, responsible for slowly responding to a fetch of a javascript
#      file.
#   3. Webdriver thread, instructs Cobalt to navigate to a URL
#
# Steps in ~ chronological order:
#   1. Create a TCP socket and listen on all interfaces.
#   2. Start Cobalt, and point it to the socket created in Step 1.
#   3. Send 3 deep links.
#   4. Load & run the javascript resource.
#   5. Check to see if JSTestsSucceeded(), receiving only the last deep link.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import,g-bad-import-order

import inspect
import logging
import os
import threading
import time
import traceback

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import MakeRequestHandlerClass
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

import SimpleHTTPServer
import urlparse

_DEEP_LINKS_HTML = 'deep_links.html'
_DEEP_LINKS_JS = 'deep_links.js'
_MAX_ALLOTTED_TIME_SECONDS = 60

_script_loading_signal = threading.Event()

# The base path of the requested assets is the parent directory.
_SERVER_ROOT_PATH = os.path.join(os.path.dirname(__file__), os.pardir)


class JavascriptRequestDetector(MakeRequestHandlerClass(_SERVER_ROOT_PATH)):
  """Proxies everything to SimpleHTTPRequestHandler, except some paths."""

  def do_GET(self):  # pylint: disable=invalid-name
    """Handles HTTP GET requests for resources."""

    parsed_path = urlparse.urlparse(self.path)
    if parsed_path.path == '/testdata/' + _DEEP_LINKS_JS:
      # It is important not to send any response back, so we block.
      logging.info('Waiting on links to be fired.')
      _script_loading_signal.wait()
      # Sending deep links is not instant on all platforms.
      # Wait some time to ensure the links are all actually received.
      logging.info('Links have been fired. Waiting...')
      time.sleep(1)
      logging.info('Done Waiting. Getting JS.')

    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


class DeepLink(black_box_tests.BlackBoxTestCase):
  """Tests firing deep links before web module is loaded."""

  def _LoadPage(self, webdriver, url):
    """Instructs webdriver to navigate to url."""
    try:
      # Note: The following is a blocking request, and returns only when the
      # page has fully loaded.  In this test, the page will not fully load
      # so, this does not return until Cobalt exits.
      webdriver.get(url)
    except:  # pylint: disable=bare-except
      traceback.print_exc()

  def _SendLink(self, query_parameter=''):
    """Test sending links into Cobalt after it's started."""

    logging.info('[ RUN ] ' + inspect.stack()[1][3] + ' ' + query_parameter)
    # Step 2. Start Cobalt, and point it to the socket created in Step 1.
    try:
      with ThreadedWebServer(JavascriptRequestDetector,
                             self.GetBindingAddress()) as server:
        with self.CreateCobaltRunner(url='about:blank') as runner:
          target_url = server.GetURL(file_name='../testdata/' +
                                     _DEEP_LINKS_HTML)
          if query_parameter != '':
            target_url += '?' + query_parameter
          cobalt_launcher_thread = threading.Thread(
              target=DeepLink._LoadPage,
              args=(self, runner.webdriver, target_url))
          cobalt_launcher_thread.start()

          # Step 3. Send 3 deep links
          for i in range(1, 4):
            link = 'link ' + str(i)
            logging.info('Sending link : ' + link)
            self.assertTrue(runner.SendDeepLink(link))
          logging.info('Links fired.')
          # Step 4. Load & run the javascript resource.
          _script_loading_signal.set()

          # Step 5. Check to see if JSTestsSucceeded().
          # Note that this call will check the DOM multiple times for a period
          # of time (current default is 30 seconds).
          success = runner.JSTestsSucceeded()
          if success:
            logging.info('[ OK ] ' + inspect.stack()[1][3])
          else:
            logging.info('[ FAILED ] ' + inspect.stack()[1][3])
          self.assertTrue(success)
    except:  # pylint: disable=bare-except
      traceback.print_exc()
      # Consider an exception being thrown as a test failure.
      self.assertTrue(False)
    finally:
      logging.info('Cleaning up.')
      _script_loading_signal.set()

  def _StartLink(self, query_parameter=''):
    """Test the initial link provided when starting Cobalt."""

    logging.info('[ RUN ] ' + inspect.stack()[1][3] + ' ' + query_parameter)
    with ThreadedWebServer(binding_address=self.GetBindingAddress()) as server:
      url = server.GetURL(file_name='testdata/' + _DEEP_LINKS_HTML)
      if query_parameter != '':
        url += '?' + query_parameter
      initial_deep_link = 'link 3'  # Expected by our test JS

      with self.CreateCobaltRunner(
          url=url, target_params=['--link=' + initial_deep_link]) as runner:
        success = runner.JSTestsSucceeded()
        if success:
          logging.info('[ OK ] ' + inspect.stack()[1][3])
        else:
          logging.info('[ FAILED ] ' + inspect.stack()[1][3])
        self.assertTrue(success)

  def _DelayedLink(self, query_parameter=''):
    """Test sending links into Cobalt after it's started."""

    logging.info('[ RUN ] ' + inspect.stack()[1][3] + ' ' + query_parameter)
    # Step 2. Start Cobalt, and point it to the socket created in Step 1.
    try:
      with ThreadedWebServer(JavascriptRequestDetector,
                             self.GetBindingAddress()) as server:
        with self.CreateCobaltRunner(url='about:blank') as runner:
          target_url = server.GetURL(file_name='../testdata/' +
                                     _DEEP_LINKS_HTML)
          if query_parameter != '':
            target_url += '?' + query_parameter
          cobalt_launcher_thread = threading.Thread(
              target=DeepLink._LoadPage,
              args=(self, runner.webdriver, target_url))
          cobalt_launcher_thread.start()

          # Step 3. Load & run the javascript resource.
          _script_loading_signal.set()

          # Step 4. Wait before sending the link.
          runner.WaitForJSTestsSetup()

          # Step 5. Send deep link
          link = 'link 3'  # Expected by our test JS
          logging.info('Sending link : ' + link)
          self.assertTrue(runner.SendDeepLink(link))
          logging.info('Links fired.')

          # Step 6. Check to see if JSTestsSucceeded().
          # Note that this call will check the DOM multiple times for a period
          # of time (current default is 30 seconds).
          success = runner.JSTestsSucceeded()
          if success:
            logging.info('[ OK ] ' + inspect.stack()[1][3])
          else:
            logging.info('[ FAILED ] ' + inspect.stack()[1][3])
          self.assertTrue(success)
    except:  # pylint: disable=bare-except
      traceback.print_exc()
      # Consider an exception being thrown as a test failure.
      self.assertTrue(False)
    finally:
      logging.info('Cleaning up.')
      _script_loading_signal.set()

  def test_delayed_link_unconsumed(self):
    """Test that a deep link does not have to be consumed."""
    return self._DelayedLink('consumed&delayedlink')

  def test_delayed_link_and_consume(self):
    """Test that a deep link is received."""
    return self._DelayedLink('consume&delayedlink')

  def test_send_link_unconsumed(self):
    """Test that the link does not have to be consumed."""
    return self._SendLink()

  def test_send_link_and_consume(self):
    """Test that the last link is received."""
    return self._SendLink('consume')

  def test_send_link_and_consume_with_initial_deep_link(self):
    """Test that the link is visible in initialDeepLink."""
    return self._SendLink('consume&initial')

  def test_send_link_and_navigate_and_consume(self):
    """Test that the link is received after navigating."""
    return self._SendLink('navigate_immediate')

  def test_send_link_and_navigate_with_delay_and_consume(self):
    """Test that the link is received after navigating with a delay."""
    return self._SendLink('navigate_delayed')

  def test_send_link_and_navigate_and_consume_with_initial_deep_link(self):
    """Test that the link is received in initialDeepLink after navigating."""
    return self._SendLink('navigate_immediate&initial')

  def test_send_link_and_navigate_with_delay_and_consume_with_initial_deep_link(
      self):
    """Test that the link is received in initialDeepLink after navigating with a delay."""
    return self._SendLink('navigate_delayed&initial')

  def test_send_link_and_consume_and_navigate(self):
    """Test that a consumed link is not received again after navigating."""
    return self._SendLink('consume&navigate_immediate')

  def test_send_link_and_consume_and_navigate_with_delay(self):
    """Test that a consumed link is not received again after navigating with a delay."""
    return self._SendLink('consume&navigate_delayed')

  def test_send_link_and_consume_with_initial_deep_link_and_navigate(self):
    """Test that a link consumed with initialDeepLink is not received again after navigating."""
    return self._SendLink('consume&initial&navigate_immediate')

  def test_start_link_unconsumed(self):
    """Test that the link does not have to be consumed."""
    return self._StartLink()

  def test_start_link_and_consume(self):
    """Test that the last link is received."""
    return self._StartLink('consume')

  def test_start_link_and_consume_with_initial_deep_link(self):
    """Test that the link is visible in initialDeepLink."""
    return self._StartLink('consume&initial')

  def test_start_link_and_navigate_and_consume(self):
    """Test that the link is received after navigating."""
    return self._StartLink('navigate_immediate')

  def test_start_link_and_navigate_with_delay_and_consume(self):
    """Test that the link is received after navigating with a delay."""
    return self._StartLink('navigate_delayed')

  def test_start_link_and_navigate_and_consume_with_initial_deep_link(self):
    """Test that the link is received in initialDeepLink after navigating."""
    return self._StartLink('navigate_immediate&initial')

  def test_start_link_and_navigate_with_delay_and_consume_with_initial_deep_link(
      self):
    """Test that the link is received in initialDeepLink after navigating with a delay."""
    return self._StartLink('navigate_delayed&initial')

  def test_start_link_and_consume_and_navigate(self):
    """Test that a consumed link is not received again after navigating."""
    return self._StartLink('consume&navigate_immediate')

  def test_start_link_and_consume_and_navigate_with_delay(self):
    """Test that a consumed link is not received again after navigating with a delay."""
    return self._StartLink('consume&navigate_delayed')

  def test_start_link_and_consume_with_initial_deep_link_and_navigate(self):
    """Test that a link consumed with initialDeepLink is not received again after navigating."""
    return self._StartLink('consume&initial&navigate_immediate')
