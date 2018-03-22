#!/usr/bin/env python
#
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

"""Example script for getting the Net log."""

from __future__ import print_function

import argparse
import pprint
import socket
import sys
import time
import threading

class NetLog:
  """
  Uses a non-blocking socket to establish a connection with a
  NetLog and then will allow the log to be fetched.
  """
  def __init__(self, host, port):
    self.host = host
    self.port = port
    self.server_socket = None

  def FetchLog(self):
    if not self.server_socket:
      self.server_socket = self._TryCreateSocketConnection()
      if not self.server_socket:
        return None
    try:
      log = self._TryRead()
      if log is None:
        self.server_socket = None
      else:
        return log
    except socket.error as (err_no, err_str):
      print(err_no, err_str)
      self.server_socket = None
      return None

  # Private members
  def _TryRead(self):
    try:
      result = self.server_socket.recv(1024)
      # An empty string is a flag that the connection has closed.
      if len(result) == 0:
        return None

      return result
    except socket.error as (err_no, err_str):
      if err_no == 10035:  # Data not ready yet.
        return ''
      else:
        raise

  def _TryCreateSocketConnection(self):
    try:
      print("Waiting for connection to " + str(self.host) + ':' + str(self.port))

      server_socket = socket.create_connection((self.host, self.port), timeout = 1)
      server_socket.setblocking(0)
      return server_socket
    except socket.timeout as err:
      return None
    except socket.error as err:
      return None


class NetLogThread(threading.Thread):
  """Threaded version of NetLog"""
  def __init__(self, host, port):
    super(NetLogThread, self).__init__()
    self.web_log = NetLog(host, port)
    self.alive = True
    self.log_mutex = threading.Lock()
    self.log = []
    self.start()

  def join(self):
    self.alive = False
    return super(NetLogThread, self).join()

  def GetLog(self):
    with self.log_mutex:
      log_copy = self.log
      self.log = []
      return ''.join(log_copy)

  def run(self):
    while self.alive:
      new_log = self.web_log.FetchLog()
      if new_log:
        with self.log_mutex:
          self.log.extend(new_log)

def TestNetLog(host, port):
  print("Started...")
  web_log = NetLog(host, port)

  while True:
    log = web_log.FetchLog()
    if log:
      print(log, end='')
    time.sleep(.1)


def main(argv):
  parser = argparse.ArgumentParser(description = 'Connects to the weblog.')
  parser.add_argument('--host', type=str, required = False,
                      default = 'localhost',
                      help = "Example localhost or 1.2.3.4")
  parser.add_argument('--port', type=int, required = False, default = '49353')
  args = parser.parse_args(argv)

  thread = NetLogThread(args.host, args.port)

  try:
    while True:
      print(thread.GetLog(), end='')
      time.sleep(.1)
  except KeyboardInterrupt as ki:
    print("\nUser canceled.")
    pass

  print("Waiting to join...")
  thread.join()
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
