# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse
import os
import struct
import sys
import warnings

# Ignore deprecation warnings, they make our output more cluttered.
warnings.filterwarnings("ignore", category=DeprecationWarning)

if sys.platform == 'win32':
  import msvcrt


class Error(Exception):
  """Error class for this module."""


class OptionError(Error):
  """Error for bad command line options."""


class FileMultiplexer(object):
  def __init__(self, fd1, fd2) :
    self.__fd1 = fd1
    self.__fd2 = fd2

  def __del__(self) :
    if self.__fd1 != sys.stdout and self.__fd1 != sys.stderr:
      self.__fd1.close()
    if self.__fd2 != sys.stdout and self.__fd2 != sys.stderr:
      self.__fd2.close()

  def write(self, text) :
    self.__fd1.write(text)
    self.__fd2.write(text)

  def flush(self) :
    self.__fd1.flush()
    self.__fd2.flush()


def MultiplexerHack(std_fd, log_fd):
  """Creates a FileMultiplexer that will write to both specified files.

  When running on Windows XP bots, stdout and stderr will be invalid file
  handles, so log_fd will be returned directly.  (This does not occur if you
  run the test suite directly from a console, but only if the output of the
  test executable is redirected.)
  """
  if std_fd.fileno() <= 0:
    return log_fd
  return FileMultiplexer(std_fd, log_fd)


class TestServerRunner(object):
  """Runs a test server and communicates with the controlling C++ test code.

  Subclasses should override the create_server method to create their server
  object, and the add_options method to add their own options.
  """

  def __init__(self):
    self.option_parser = optparse.OptionParser()
    self.add_options()

  def main(self):
    self.options, self.args = self.option_parser.parse_args()

    logfile = open('testserver.log', 'w')
    sys.stderr = MultiplexerHack(sys.stderr, logfile)
    if self.options.log_to_console:
      sys.stdout = MultiplexerHack(sys.stdout, logfile)
    else:
      sys.stdout = logfile

    server_data = {
      'host': self.options.host,
    }
    self.server = self.create_server(server_data)
    self._notify_startup_complete(server_data)
    self.run_server()

  def create_server(self, server_data):
    """Creates a server object and returns it.

    Must populate server_data['port'], and can set additional server_data
    elements if desired."""
    raise NotImplementedError()

  def run_server(self):
    try:
      self.server.serve_forever()
    except KeyboardInterrupt:
      print 'shutting down server'
      self.server.stop = True

  def add_options(self):
    self.option_parser.add_option('--startup-pipe', type='int',
                                  dest='startup_pipe',
                                  help='File handle of pipe to parent process')
    self.option_parser.add_option('--log-to-console', action='store_const',
                                  const=True, default=False,
                                  dest='log_to_console',
                                  help='Enables or disables sys.stdout logging '
                                  'to the console.')
    self.option_parser.add_option('--port', default=0, type='int',
                                  help='Port used by the server. If '
                                  'unspecified, the server will listen on an '
                                  'ephemeral port.')
    self.option_parser.add_option('--host', default='127.0.0.1',
                                  dest='host',
                                  help='Hostname or IP upon which the server '
                                  'will listen. Client connections will also '
                                  'only be allowed from this address.')

  def _notify_startup_complete(self, server_data):
    # Notify the parent that we've started. (BaseServer subclasses
    # bind their sockets on construction.)
    if self.options.startup_pipe is not None:
      server_data_json = json.dumps(server_data)
      server_data_len = len(server_data_json)
      print 'sending server_data: %s (%d bytes)' % (
        server_data_json, server_data_len)
      if sys.platform == 'win32':
        fd = msvcrt.open_osfhandle(self.options.startup_pipe, 0)
      else:
        fd = self.options.startup_pipe
      startup_pipe = os.fdopen(fd, "w")
      # First write the data length as an unsigned 4-byte value.  This
      # is _not_ using network byte ordering since the other end of the
      # pipe is on the same machine.
      startup_pipe.write(struct.pack('=L', server_data_len))
      startup_pipe.write(server_data_json)
      startup_pipe.close()
