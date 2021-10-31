#!/usr/bin/env python2

# Copyright 2017 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Send a link to a process running LinkReceiver.

If a Starboard implementation instantiates
starboard::shared::starboard::LinkReceiver, then it will write a file containing
the port to the configured temporary directory. This script finds that file and
sends the specified link to that service. It is up to the application to
interpret that link content.

Note that LinkReceiver generally runs on the loopback interface, so this script
should be run from the same device running the application.

"""

import argparse
from contextlib import closing
import logging
import os
import socket
import sys
import tempfile
import textwrap
import time


def _Uncase(text):
  return text.upper().lower()


def _GetPids(executable):
  if sys.platform in ('win32', 'cygwin'):
    raise NotImplementedError('Implement me for Windows!')
  elif sys.platform.startswith('linux') or sys.platform == 'darwin':
    pids = []
    executable = _Uncase(executable)
    for pid in os.listdir('/proc'):
      if pid == 'curproc':
        continue

      pid_path = '/proc/' + pid
      if not os.path.isdir(pid_path):
        continue

      try:
        cmdline_path = os.path.join(pid_path, 'cmdline')
        with open(cmdline_path, mode='rb') as cmdline:
          content = cmdline.read().decode().split('\x00')
      except IOError:
        continue

      if os.path.basename(_Uncase(content[0])).startswith(executable):
        pids += [pid]
    return pids


def _FindTemporaryFile(prefix, suffix):
  directory = tempfile.gettempdir()
  for entry in os.listdir(directory):
    caseless_entry = _Uncase(entry)
    if caseless_entry.startswith(prefix) and caseless_entry.endswith(suffix):
      return os.path.join(directory, entry)

  return None


def _ConnectWithRetry(s, port, num_attempts):
  for attempt in range(num_attempts):
    if attempt > 0:
      time.sleep(1)
    try:
      s.connect(('localhost', port))
      return True
    except (RuntimeError, IOError):
      logging.error('Could not connect to port %d, attempt %d / %d', port,
                    attempt, num_attempts)
  return False


def SendLink(executable, link, connection_attempts=1):
  """Sends a link to the process starting with the given executable name."""

  pids = _GetPids(executable)

  if not pids:
    logging.error('No PIDs found for %s', executable)
    return 1

  if len(pids) > 1:
    logging.error('Multiple PIDs found for %s: %s', executable, pids)
    return 1

  pid = pids[0]
  temporary_directory = _FindTemporaryFile(_Uncase(executable), _Uncase(pid))
  if not os.path.isdir(temporary_directory):
    logging.error('Not a directory: %s', temporary_directory)
    return 1

  port_file_path = os.path.join(temporary_directory, 'link_receiver_port')
  if not os.path.isfile(port_file_path):
    logging.error('Not a file: %s', port_file_path)
    return 1

  try:
    with open(port_file_path, mode='rb') as port_file:
      port = int(port_file.read().decode())
  except IOError:
    logging.exception('Could not open port file: %s', port_file_path)
    return 1

  try:
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
      if not _ConnectWithRetry(s, port, connection_attempts):
        logging.exception('Could not connect to port: %d', port)
        return 1
      terminated_link = link + '\x00'
      bytes_sent = 0
      while bytes_sent < len(terminated_link):
        sent = s.send(terminated_link[bytes_sent:])
        if sent == 0:
          raise RuntimeError('Connection terminated by remote host.')
        bytes_sent += sent
  except (RuntimeError, IOError):
    logging.exception('Could not connect to port: %d', port)
    return 1

  logging.info('Link "%s" sent to %s at pid %s on port %d.', link, executable,
               pid, port)

  return 0


def main():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      description=textwrap.dedent(__doc__))
  parser.add_argument(
      'executable',
      type=str,
      help='Name of the running executable to send a link to.')
  parser.add_argument(
      'link', type=str, help='The link content to send to the executable.')
  arguments = parser.parse_args()
  return SendLink(arguments.executable, arguments.link)


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO, format='[%(levelname)5s] %(message)s')
  sys.exit(main())
