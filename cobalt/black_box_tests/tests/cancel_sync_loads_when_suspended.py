# Copyright 2018 Google Inc. All Rights Reserved.
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
"""Tests cancelation of synchronous loading of scripts on Suspend."""

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
#   3. Wait HTTP request for html resource.
#   4. Respond to a HTTP request.
#   5. Wait for a request for javascript resource.
#   6. Suspend Cobalt process.
#   7. Cobalt disconnects from the socket.
#   8. Resume Cobalt process, which enables the JS test to pass.
#   9. Check to see if JSTestsSucceeded().

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import _env  # pylint: disable=unused-import,g-bad-import-order

import os
import SimpleHTTPServer
import threading
import traceback
import urlparse

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import MakeRequestHandlerClass
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

_CANCEL_SYNC_LOADS_WHEN_SUSPENDED_HTML = 'cancel_sync_loads_when_suspended.html'
_CANCEL_SYNC_LOADS_WHEN_SUSPENDED_JS = 'cancel_sync_loads_when_suspended.js'
_MAX_ALLOTTED_TIME_SECONDS = 60

_received_script_resource_request = threading.Event()
_test_finished = threading.Event()

# The base path of the requested assets is the parent directory.
_SERVER_ROOT_PATH = os.path.join(os.path.dirname(__file__), os.pardir)


class JavascriptRequestDetector(MakeRequestHandlerClass(_SERVER_ROOT_PATH)):
  """Proxies everything to SimpleHTTPRequestHandler, except some paths."""

  def do_GET(self):  # pylint: disable=invalid-name
    """Handles HTTP GET requests for resources."""

    parsed_path = urlparse.urlparse(self.path)
    if parsed_path.path == '/testdata/' + _CANCEL_SYNC_LOADS_WHEN_SUSPENDED_JS:
      _received_script_resource_request.set()
      # It is important not to send any response back, so we block.
      print('Waiting on test to finish.')
      _test_finished.wait()
      print('Test is finished.')
      return

    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


class CancelSyncLoadsWhenSuspended(black_box_tests.BlackBoxTestCase):
  """Tests cancelation of synchronous loading of scripts on Suspend."""

  def _LoadPage(self, webdriver, url):
    """Instructs webdriver to navigate to url."""
    try:
      # Note: The following is a blocking request, and returns only when the
      # page has fully loaded.  In this test, the page will not fully load
      # so, this does not return until Cobalt exits.
      webdriver.get(url)
    except:  # pylint: disable=bare-except
      traceback.print_exc()

  def test_simple(self):

    # Step 2. Start Cobalt, and point it to the socket created in Step 1.
    try:
      with ThreadedWebServer(
          JavascriptRequestDetector) as server, self.CreateCobaltRunner(
              url='about:blank') as runner:
        target_url = server.GetURL(file_name='../testdata/' +
                                   _CANCEL_SYNC_LOADS_WHEN_SUSPENDED_HTML)
        cobalt_launcher_thread = threading.Thread(
            target=CancelSyncLoadsWhenSuspended._LoadPage,
            args=(self, runner.webdriver, target_url))
        cobalt_launcher_thread.start()

        # Step 3. Wait HTTP request for html resource.
        print('Waiting for script resource request')
        request_received = _received_script_resource_request.wait(
            _MAX_ALLOTTED_TIME_SECONDS)
        print('Request received: {}'.format(request_received))
        # Step 5. Wait for a request for javascript resource.
        self.assertTrue(request_received)

        # Step 6. Suspend Cobalt process.
        print('Suspending Cobalt.')
        runner.SendSuspend()
        # Step 7. Cobalt disconnects from the socket.
        # Step 8. Resume Cobalt process, which enables the JS test to pass.
        print('Resuming Cobalt.')
        runner.SendResume()

        # Step 9. Check to see if JSTestsSucceeded().
        # Note that this call will check the DOM multiple times for a period of
        # time (current default is 30 seconds).
        self.assertTrue(runner.JSTestsSucceeded())
    except:  # pylint: disable=bare-except
      traceback.print_exc()
      # Consider an exception being thrown as a test failure.
      self.assertTrue(False)
    finally:
      print('Cleaning up.')
      _test_finished.set()
