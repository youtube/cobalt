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

"""Example script for getting the Net Args."""

from __future__ import print_function

import pprint

import argparse
import socket
import sys
import thread
import time
import threading

# Returns |True| if a connection was made and the NetArg payload was delivered.
# Example:
#  TryConnectAndSendNetArgs('1.2.3.4', '1234', ['--argument', '--switch=value'])
def TryConnectAndSendNetArgs(host, port, arg_list):
  arg_string = '\n'.join(arg_list)

  try:
    server_socket = socket.create_connection((host, port), timeout = .5)
    result = server_socket.sendall(arg_string)
    server_socket.close()
    return True
  except socket.timeout as err:
    return False
  except socket.error as (err_no, err_str):
    print(err_no, err_str)
    return False

class NetArgsThread(threading.Thread):
  """Threaded version of NetArgs"""

  def __init__(self, host, port, arg_list):
    super(NetArgsThread, self).__init__()
    assert isinstance(arg_list, list)
    self.host = host
    self.port = port
    self.args_sent = False
    self.join_called = False
    self.arg_list = arg_list
    self.mutex = threading.Lock()
    self.start()

  def join(self):
    with self.mutex:
      self.join_called = True
    return super(NetArgsThread, self).join()

  def ArgsSent(self):
    with self.mutex:
      return self.args_sent

  def run(self):
    while True:
      with self.mutex:
        if self.join_called:
          break
      connected_and_sent = TryConnectAndSendNetArgs(
          self.host, self.port, self.arg_list)
      if connected_and_sent:
        with self.mutex:
          self.args_sent = True
          break

def main(argv):
  parser = argparse.ArgumentParser(description = 'Connects to the weblog.')
  parser.add_argument('--host', type=str, required = False,
                      default = 'localhost',
                      help = "Example localhost or 1.2.3.4")
  parser.add_argument('--port', type=int, required = False, default = '49354')
  parser.add_argument('--arg', type=str, required = True)
  args = parser.parse_args(argv)

  net_args_thread = NetArgsThread(
          args.host, args.port, [args.arg])

  while not net_args_thread.ArgsSent():
    print("Waiting to send arg " + args.arg + " to " + str(args.host) +
          ":" + str(args.port) + "...")
    time.sleep(.5)

  print("Argument", args.arg, "was sent to", \
        args.host + ":" + str(args.port))

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
