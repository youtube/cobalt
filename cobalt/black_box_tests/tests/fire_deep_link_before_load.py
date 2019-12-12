# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Tests sending deep links before load."""

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
#   5. Check to see if JSTestsSucceeded().

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

_FIRE_DEEP_LINK_BEFORE_LOAD_HTML = 'fire_deep_link_before_load.html'
_FIRE_DEEP_LINK_BEFORE_LOAD_JS = 'fire_deep_link_before_load.js'
_MAX_ALLOTTED_TIME_SECONDS = 60

_links_fired = threading.Event()

# The base path of the requested assets is the parent directory.
_SERVER_ROOT_PATH = os.path.join(os.path.dirname(__file__), os.pardir)


class JavascriptRequestDetector(MakeRequestHandlerClass(_SERVER_ROOT_PATH)):
  """Proxies everything to SimpleHTTPRequestHandler, except some paths."""

  def do_GET(self):  # pylint: disable=invalid-name
    """Handles HTTP GET requests for resources."""

    parsed_path = urlparse.urlparse(self.path)
    if parsed_path.path == '/testdata/' + _FIRE_DEEP_LINK_BEFORE_LOAD_JS:
      # It is important not to send any response back, so we block.
      print('Waiting on links to be fired.')
      _links_fired.wait()
      print('Links have been fired. Getting JS.')

    return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


class FireDeepLinkBeforeLoad(black_box_tests.BlackBoxTestCase):
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

  def test_simple(self):

    # Step 2. Start Cobalt, and point it to the socket created in Step 1.
    try:
      with ThreadedWebServer(JavascriptRequestDetector,
                             self.GetBindingAddress()) as server:
        with self.CreateCobaltRunner(url='about:blank') as runner:
          target_url = server.GetURL(file_name='../testdata/' +
                                     _FIRE_DEEP_LINK_BEFORE_LOAD_HTML)
          cobalt_launcher_thread = threading.Thread(
              target=FireDeepLinkBeforeLoad._LoadPage,
              args=(self, runner.webdriver, target_url))
          cobalt_launcher_thread.start()

          # Step 3. Send 3 deep links
          for i in range(1, 4):
            link = 'link ' + str(i)
            print('Sending link : ' + link)
            self.assertTrue(runner.SendDeepLink(link) == 0)
          print('Links fired.')
          # Step 4. Load & run the javascript resource.
          _links_fired.set()

          # Step 5. Check to see if JSTestsSucceeded().
          # Note that this call will check the DOM multiple times for a period
          # of time (current default is 30 seconds).
          self.assertTrue(runner.JSTestsSucceeded())
    except:  # pylint: disable=bare-except
      traceback.print_exc()
      # Consider an exception being thrown as a test failure.
      self.assertTrue(False)
    finally:
      print('Cleaning up.')
      _links_fired.set()
