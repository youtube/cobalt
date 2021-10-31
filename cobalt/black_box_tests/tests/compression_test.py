# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Tests if Cobalt properly handles compressed files."""

import os
import zlib

try:
  import brotli
except ImportError:
  brotli = None
from SimpleHTTPServer import SimpleHTTPRequestHandler
from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer


def encode(raw_content, encoding_type):
  """Encode content using the algorithm specified by encoding_type."""
  if encoding_type == 'gzip':
    return encode_gzip(raw_content)
  elif encoding_type == 'deflate':
    return encode_deflate(raw_content)
  elif encoding_type == 'br':
    return encode_brotli(raw_content)
  raise ValueError('Unknown encoding type used [{}].'.format(encoding_type))


def encode_gzip(raw_content):
  compressor = zlib.compressobj(9, zlib.DEFLATED, 16 + zlib.MAX_WBITS)
  content = compressor.compress(raw_content) + compressor.flush()
  return content


def encode_deflate(raw_content):
  compressor = zlib.compressobj(9, zlib.DEFLATED)
  content = compressor.compress(raw_content) + compressor.flush()
  return content


def encode_brotli(raw_content):
  return brotli.compress(raw_content)


def make_request_handler_class(base_path, encoding_type):
  """RequestHandler that serves files that reside relative to base_path.

  Args:
    base_path: A path considered to be the root directory.
    encoding_type: The content encoding type to use to compress served files.

  Returns:
    A RequestHandler class.
  """

  class TestDataHTTPRequestHandler(SimpleHTTPRequestHandler):
    """Handles HTTP requests and serves compressed responses.

    Encodes response files regardless of the 'Accept-Encoding' header in the
    request. For testing purposes only.
    """

    _current_working_directory = os.getcwd()
    _base_path = base_path
    _encoding_type = encoding_type

    def do_GET(self):  # pylint: disable=invalid-name
      content = self.get_content()
      self.wfile.write(content)

    def do_HEAD(self):  # pylint: disable=invalid-name
      # Run get_content to send the headers, but ignore the returned results
      self.get_content()

    def translate_path(self, path):
      """Translate the request path to the file in the testdata directory."""

      potential_path = SimpleHTTPRequestHandler.translate_path(self, path)
      potential_path = potential_path.replace(self._current_working_directory,
                                              self._base_path)
      return potential_path

    def get_content(self):
      """Gets the contents of the file at path, encodes and returns it."""
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
      self.send_header('Content-Encoding', self._encoding_type)

      content = encode(raw_content, self._encoding_type)
      content_length = len(content)

      self.send_header('Content-Length', content_length)
      self.send_header('Last-Modified', modified_date_time)
      self.end_headers()

      return content

  return TestDataHTTPRequestHandler


def execute_test(test_runner, encoding_type):
  path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
  with ThreadedWebServer(
      binding_address=test_runner.GetBindingAddress(),
      handler=make_request_handler_class(path, encoding_type)) as server:
    url = server.GetURL(file_name='testdata/compression.html')

    with test_runner.CreateCobaltRunner(url=url) as runner:
      test_runner.assertTrue(runner.JSTestsSucceeded())


class CompressionTest(black_box_tests.BlackBoxTestCase):
  """Verify correct decompression of compressed files."""

  def test_gzip(self):
    execute_test(self, 'gzip')

  def test_deflate(self):
    execute_test(self, 'deflate')

  def test_brotli(self):
    # Skip this test if the brotli module is not available.
    if brotli:
      execute_test(self, 'br')
