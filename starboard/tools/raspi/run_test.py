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

# Timeouts are in seconds
_PEXPECT_DEFAULT_TIMEOUT = 600
_PEXPECT_EXPECT_TIMEOUT = 60


def _CleanupProcess(process):
  """Closes current pexpect process.

  Args:
    process: Current pexpect process.
  """
  if process is not None and process.isalive():
    # Send ctrl-c to the raspi.
    process.sendline(chr(3))
    process.close()


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
  _CleanupProcess(process)
  sys.exit(signum)


def _RunTest(test_path, raspi_ip, flags):
  """Run a test on a Raspi machine and print relevant stdout.

  Args:
    test_path: path to the binary test file to be run
    raspi_ip: the IP address of the Raspi device to run the test on
    flags: list of flags to pass into binary.

  Returns:
    Exit status of running the test.
  """
  return_value = 1

  try:
    process = None

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
    process = pexpect.spawn(rsync_command, timeout=_PEXPECT_DEFAULT_TIMEOUT)

    signal.signal(signal.SIGINT,
                  functools.partial(_SigIntOrSigTermHandler, process))
    signal.signal(signal.SIGTERM,
                  functools.partial(_SigIntOrSigTermHandler, process))

    process.expect(r'\S+ password:', timeout=_PEXPECT_EXPECT_TIMEOUT)
    process.sendline(_RASPI_PASSWORD)

    while True:
      line = sanitize_line_re.sub('', process.readline())
      if line:
        sys.stdout.write(line)
        sys.stdout.flush()
      else:
        break

    # ssh into the raspi and run the test
    ssh_command = 'ssh ' + raspi_user_hostname
    process = pexpect.spawn(ssh_command, timeout=_PEXPECT_DEFAULT_TIMEOUT)

    signal.signal(signal.SIGINT,
                  functools.partial(_SigIntOrSigTermHandler, process))
    signal.signal(signal.SIGTERM,
                  functools.partial(_SigIntOrSigTermHandler, process))

    process.expect(r'\S+ password:', timeout=_PEXPECT_EXPECT_TIMEOUT)
    process.sendline(_RASPI_PASSWORD)

    # Escape command line metacharacters in the flags
    meta_chars = '()[]{}%!^"<>&|'
    meta_re = re.compile('(' + '|'.join(
        re.escape(char) for char in list(meta_chars)) + ')')
    escaped_flags = re.subn(meta_re, r'\\\1', flags)[0]

    test_command = raspi_test_path + ' ' + escaped_flags
    test_time_tag = 'TEST-{time}'.format(time=time.time())
    test_success_tag = 'succeeded'
    test_failure_tag = 'failed'
    test_success_output = ' && echo ' + test_time_tag + ' ' + test_success_tag
    test_failure_output = ' || echo ' + test_time_tag + ' ' + test_failure_tag
    process.sendline(test_command + test_success_output + test_failure_output)

    while True:
      line = sanitize_line_re.sub('', process.readline())
      if not line:
        break
      sys.stdout.write(line)
      sys.stdout.flush()
      if line.startswith(test_time_tag):
        if line.find(test_success_tag) != -1:
          return_value = 0
        break

  except ValueError:
    logging.exception('Test path invalid.')
  except pexpect.EOF:
    logging.exception('pexpect encountered EOF while reading line.')
  except pexpect.TIMEOUT:
    logging.exception('pexpect timed out while reading line.')
  # pylint: disable=W0703
  except Exception:
    logging.exception('Error occured while running test.')
  finally:
    _CleanupProcess(process)

  return return_value


def _AddTargetFlag(parser):
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


def _AddFlagsFlag(parser, default=None):
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

  _AddFlagsFlag(parser)
  _AddTargetFlag(parser)
  parser.add_argument('test_path', help='Path of test to be run.', type=str)

  args = parser.parse_args()

  return _RunTest(args.test_path, raspi_ip=args.target, flags=args.flags)


if __name__ == '__main__':
  sys.exit(main())
