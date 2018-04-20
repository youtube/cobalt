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
"""Contains a threaded web server used for serving testdata."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import SimpleHTTPServer
import socket
import SocketServer
import threading


class _ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
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

  return TestDataHTTPRequestHandler


class ThreadedWebServer(object):
  """A HTTP WebServer that serves requests in a separate thread."""

  def __init__(self,
               handler=MakeRequestHandlerClass(os.path.dirname(__file__))):
    _ThreadedTCPServer.allow_reuse_address = True
    # Get the socket address for the ANY interface.  Doing it this way
    # has it so that it will work for IPv4, and IPv6 only networks.
    # Note that putting '::' as the hostname does not work at this time
    # (see https://bugs.python.org/issue20215).  Instead, the following code
    # was inspired by https://docs.python.org/2/library/socket.html.
    for result in socket.getaddrinfo(None, 0, socket.AF_UNSPEC,
                                     socket.SOCK_STREAM, 0, socket.AI_PASSIVE):
      # This is (0.0.0.0, 0) or equivalent in IPv6 (could be more than 2
      # elements).
      socket_address = result[4]
      break

    self._server = _ThreadedTCPServer(socket_address, handler)
    self._server_thread = None

    self._bound_port = self._server.server_address[1]
    address_pack_list = socket.getaddrinfo(socket.gethostname(),
                                           self._bound_port)
    first_address_pack = address_pack_list[0]
    self._bound_ip, _ = first_address_pack[4]
    self._bound_host, _ = first_address_pack[4]

  def GetURL(self, file_name):
    """Given a |file_name|, return a HTTP URI that can be fetched.

    Args:
      file_name: a string containing a file_name.

    Returns:
      A string containing a HTTP URI.
    """
    return 'http://{}:{}/{}'.format(self._bound_host, self._bound_port,
                                    file_name)

  def __enter__(self):
    self._server_thread = threading.Thread(target=self._server.serve_forever)
    self._server_thread.start()
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    self._server.shutdown()
    self._server.server_close()
    self._server_thread.join()
