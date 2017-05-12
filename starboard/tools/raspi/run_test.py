#!/usr/bin/python2
#
# Copyright 2016 Google Inc. All Rights Reserved.
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
"""Run a test on Raspi device and print results of test to stdout."""

from __future__ import print_function

import argparse
import functools
import logging
import os
import re
import signal
import sys
import textwrap
import time

import pexpect

_COBALT_SRC = os.path.abspath(os.path.join(*([__file__] + 4 * [os.pardir])))

_RASPI_USERNAME = 'pi'
_RASPI_PASSWORD = 'raspberry'


# pylint: disable=unused-argument
def _SigIntOrSigTermHandler(process, signum, frame):
  """Clean up and exit with status |signum|.

  Args:
    process: Current pexpect process.
    signum: Signal number that triggered this callback.  Passed in when the
      signal handler is called by python runtime.
    frame: Current stack frame.  Passed in when the signal handler is called by
      python runtime.
  """
  if process.isalive():
    # Send ctrl-c to the raspi.
    process.sendline(chr(3))
    process.close()
  sys.exit(signum)


def RunTest(test_path, raspi_ip, flags):
  """Run a test on a Raspi machine and print relevant stdout.

  Args:
    test_path: path to the binary test file to be run
    raspi_ip: the IP address of the Raspi device to run the test on
    flags: list of flags to pass into binary.

  Returns:
    Exit status of running the test.

  Raises:
    ValueError: Raised if test_path is invalid.
  """
  if not os.path.isfile(test_path):
    raise ValueError('test_path ({}) must be a file.'.format(test_path))

  # This is used to strip ansi color codes from output.
  sanitize_line_re = re.compile(r'\x1b[^m]*m')

  sys.stdout.write('Process launched, ID={}\n'.format(os.getpid()))
  sys.stdout.flush()

  test_dir_path, test_file = os.path.split(test_path)
  test_base_dir = os.path.basename(os.path.normpath(test_dir_path))

  raspi_user_hostname = _RASPI_USERNAME + '@' + raspi_ip
  raspi_test_path = os.path.join(test_base_dir, test_file)

  # rsync the test files to the raspi
  options = '-avzh --exclude obj*'
  source = test_dir_path
  destination = raspi_user_hostname + ':~/'
  rsync_command = 'rsync ' + options + ' ' + source + ' ' + destination
  rsync_process = pexpect.spawn(rsync_command, timeout=120)

  signal.signal(signal.SIGINT,
                functools.partial(_SigIntOrSigTermHandler, rsync_process))
  signal.signal(signal.SIGTERM,
                functools.partial(_SigIntOrSigTermHandler, rsync_process))

  rsync_process.expect(r'\S+ password:')
  rsync_process.sendline(_RASPI_PASSWORD)

  while True:
    line = sanitize_line_re.sub('', rsync_process.readline())
    if line:
      sys.stdout.write(line)
      sys.stdout.flush()
    else:
      break

  # ssh into the raspi and run the test
  ssh_command = 'ssh ' + raspi_user_hostname
  ssh_process = pexpect.spawn(ssh_command, timeout=120)

  signal.signal(signal.SIGINT,
                functools.partial(_SigIntOrSigTermHandler, ssh_process))
  signal.signal(signal.SIGTERM,
                functools.partial(_SigIntOrSigTermHandler, ssh_process))

  ssh_process.expect(r'\S+ password:')
  ssh_process.sendline(_RASPI_PASSWORD)

  test_command = raspi_test_path + ' ' + flags
  test_end_tag = 'END_TEST-{time}'.format(time=time.time())
  ssh_process.sendline(test_command + '; echo ' + test_end_tag)

  while True:
    line = sanitize_line_re.sub('', ssh_process.readline())
    if line and not line.startswith(test_end_tag):
      sys.stdout.write(line)
      sys.stdout.flush()
    else:
      break

  return 0


def AddTargetFlag(parser):
  """Add target to argument parser."""
  parser.add_argument(
      '-t',
      '--target',
      default=None,
      help=(
          'IP address of the Raspi device to send package to.  If no value is '
          'specified for this argument then it will default to the '
          'environment variable `RASPI_ADDR\' if that is set, and '
          'otherwise exit with status code 1.'),
      nargs='?')


def AddFlagsFlag(parser, default=None):
  """Add flags to argument parser.

  Args:
    parser: An ArgumentParser instance.
    default: A string consisting of default value.
  """
  if default is None:
    default = ''
  parser.add_argument(
      '-f',
      '--flags',
      default=default,
      help=('Space separated flags that will be forwarded into Cobalt upon '
            'launch. Note that there is no way to pass in a flag that has a '
            'space in it.'),
      nargs='?')


def main():
  """Run RunTest using path to binary provided by command line arguments.

  Returns:
    exit status
  """
  sys.path.append(os.path.join(_COBALT_SRC, 'cobalt', 'build'))

  logging.basicConfig(
      level=logging.INFO,
      format='%(levelname)s|%(filename)s:%(lineno)s|%(message)s')

  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      description=textwrap.dedent(__doc__))

  AddFlagsFlag(parser)
  AddTargetFlag(parser)
  parser.add_argument('test_path', help='Path of test to be run.', type=str)

  args = parser.parse_args()

  try:
    return_value = RunTest(
        args.test_path, raspi_ip=args.target, flags=args.flags)
  # pylint: disable=W0703
  except Exception:
    logging.exception('Error occured while running binary.')
    return_value = 1

  return return_value


if __name__ == '__main__':
  sys.exit(main())
