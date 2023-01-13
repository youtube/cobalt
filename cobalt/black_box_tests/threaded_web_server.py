# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""Contains a threaded web server used for serving testdata."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import logging
import os
from six.moves import SimpleHTTPServer
from six.moves import socketserver
import socket
import threading


class _ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
  pass


def MakeRequestHandlerClass(base_path):
  """RequestHandler that serves files that reside relative to base_path.

  Args:
    base_path: A path considered to be the root directory.

  Returns:
    A RequestHandler class.
  """

  class TestDataHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """Handles HTTP requests, but changes the base directory for assets."""

    _current_working_directory = os.getcwd()
    _base_path = base_path

    def translate_path(self, path):
      """Translate the request path to the file in the testdata directory."""

      potential_path = SimpleHTTPServer.SimpleHTTPRequestHandler.translate_path(
          self, path)
      potential_path = potential_path.replace(self._current_working_directory,
                                              self._base_path)
      return potential_path

    def log_message(self, format_string, *args):
      logging.info(format_string % (args))  # pylint: disable=logging-not-lazy

  return TestDataHTTPRequestHandler


def MakeCustomHeaderRequestHandlerClass(base_path, paths_to_headers):
  """RequestHandler that serves files that reside relative to base_path.

  Args:
    base_path: A path considered to be the root directory.
    paths_to_headers: A dictionary with keys partial paths and values of header
    dicts. Key is expected to be a substring of a file being served.

    E.g. if you have a test with files foo.html and foo.js, you can serve them
    separate headers with the following:
    paths_to_headers = {
      'foo.html': {
        'Content-Security-Policy', "script-src 'unsafe-inline';"
      },
      'foo.js': {'Content-Security-Policy', "default-src 'self';"}
    }

  Returns:
    A RequestHandler class.
  """

  class TestDataHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """Handles HTTP requests and serves responses with specified headers.

    For testing purposes only.
    """

    _current_working_directory = os.getcwd()
    _base_path = base_path
    _paths_to_headers = paths_to_headers

    def do_GET(self):  # pylint: disable=invalid-name
      content = self.get_content()
      if content:
        self.wfile.write(content)

    def do_HEAD(self):  # pylint: disable=invalid-name
      # Run get_content to send the headers, but ignore the returned results
      self.get_content()

    def translate_path(self, path):
      """Translate the request path to the file in the testdata directory."""

      potential_path = SimpleHTTPServer.SimpleHTTPRequestHandler.translate_path(
          self, path)
      potential_path = potential_path.replace(self._current_working_directory,
                                              self._base_path)
      return potential_path

    def get_content(self):
      """Returns the contents of the file at path with added headers."""
      path = self.translate_path(self.path)
      modified_date_time = None
      raw_content = None
      try:
        with open(path, 'rb') as f:
          filestream = os.fstat(f.fileno())
          modified_date_time = self.date_time_string(filestream.st_mtime)
          raw_content = f.read()
      except IOError:
        self.send_error(404, 'File not found')
        return None

      content_type = self.guess_type(path)
      self.send_response(200)
      self.send_header('Content-type', content_type)
      for partial_path, headers in self._paths_to_headers.items():
        if partial_path in path:
          for header_key, header_value in headers.items():
            self.send_header(header_key, header_value)

      content_length = len(raw_content)

      self.send_header('Content-Length', content_length)
      self.send_header('Last-Modified', modified_date_time)
      self.end_headers()

      return raw_content

  return TestDataHTTPRequestHandler


class ThreadedWebServer(object):
  """A HTTP WebServer that serves requests in a separate thread."""

  def __init__(self,
               handler=MakeRequestHandlerClass(os.path.dirname(__file__)),
               binding_address=None):
    _ThreadedTCPServer.allow_reuse_address = True
    _ThreadedTCPServer.block_on_close = False

    if not binding_address:
      # No specific binding address specified. Bind to any interfaces instead.
      binding_address = '0.0.0.0'
    self._server = _ThreadedTCPServer((binding_address, 0), handler)

    self._server_thread = None

    self._bound_port = self._server.server_address[1]
    self._bound_host = self._server.server_address[0]
    if self._bound_host == '0.0.0.0':
      # When listening to any interfaces, get the IPv4 address of the hostname.
      self._bound_host = socket.gethostbyname(socket.gethostname())

  def GetURL(self, file_name):
    """Given a |file_name|, return a HTTP URI that can be fetched.

    Args:
      file_name: a string containing a file_name.

    Returns:
      A string containing a HTTP URI.
    """
    return f'http://{self._bound_host}:{self._bound_port}/{file_name}'

  def __enter__(self):
    self._server_thread = threading.Thread(target=self._server.serve_forever)
    self._server_thread.start()
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    self._server.shutdown()
    self._server.server_close()
    self._server_thread.join()
