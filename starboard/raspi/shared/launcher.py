#
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
"""Raspi implementation of Starboard launcher abstraction."""

import functools
import logging
import os
import re
import signal
import sys
import threading
import time

import _env  # pylint: disable=unused-import
import pexpect
from starboard.tools import abstract_launcher


# pylint: disable=unused-argument
def _SigIntOrSigTermHandler(signum, frame):
  """Clean up and exit with status |signum|.

  Args:
    signum: Signal number that triggered this callback.  Passed in when the
      signal handler is called by python runtime.
    frame: Current stack frame.  Passed in when the signal handler is called by
      python runtime.
  """
  sys.exit(signum)


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt/tools on Raspi."""

  _STARTUP_TIMEOUT_SECONDS = 1800

  _RASPI_USERNAME = 'pi'
  _RASPI_PASSWORD = 'raspberry'
  _SSH_LOGIN_SIGNAL = 'cobalt-launcher-login-success'

  # pexpect times out each second to allow Kill to quickly stop a test run
  _PEXPECT_TIMEOUT = 1

  # Wait up to 30 seconds for the password prompt from the raspi
  _PEXPECT_PASSWORD_TIMEOUT_MAX_RETRIES = 30
  # Wait up to 900 seconds for new output from the raspi
  _PEXPECT_READLINE_TIMEOUT_MAX_RETRIES = 900

  # This is used to strip ansi color codes from pexpect output.
  _PEXPECT_SANITIZE_LINE_RE = re.compile(r'\x1b[^m]*m')

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   **kwargs)
    env = os.environ.copy()
    env.update(self.env_variables)
    self.full_env = env

    if not self.device_id:
      self.device_id = self.full_env.get('RASPI_ADDR')
      if not self.device_id:
        raise ValueError(
            'Unable to determine target, please pass it in, or set RASPI_ADDR '
            'environment variable.')

    self.startup_timeout_seconds = Launcher._STARTUP_TIMEOUT_SECONDS

    self.pexpect_process = None
    self._InitPexpectCommands()

    self.run_inactive = threading.Event()
    self.run_inactive.set()

    self.shutdown_initiated = threading.Event()

    signal.signal(signal.SIGINT, functools.partial(_SigIntOrSigTermHandler))
    signal.signal(signal.SIGTERM, functools.partial(_SigIntOrSigTermHandler))

  def _InitPexpectCommands(self):
    """Initializes all of the pexpect commands needed for running the test."""

    test_dir = os.path.join(self.out_directory, 'deploy', self.target_name)
    test_file = self.target_name

    test_path = os.path.join(test_dir, test_file)
    if not os.path.isfile(test_path):
      raise ValueError('TargetPath ({}) must be a file.'.format(test_path))

    raspi_user_hostname = Launcher._RASPI_USERNAME + '@' + self.device_id

    # Use the basename of the out directory as a common directory on the device
    # so content can be reused for several targets w/o re-syncing for each one.
    raspi_test_dir = os.path.basename(self.out_directory)
    raspi_test_path = os.path.join(raspi_test_dir, test_file)

    # rsync command setup
    options = '-avzLh'
    source = test_dir + '/'
    destination = '{}:~/{}/'.format(raspi_user_hostname, raspi_test_dir)
    self.rsync_command = 'rsync ' + options + ' ' + source + ' ' + destination

    # ssh command setup
    self.ssh_command = 'ssh ' + raspi_user_hostname

    # escape command line metacharacters in the flags
    flags = ' '.join(self.target_command_line_params)
    meta_chars = '()[]{}%!^"<>&|'
    meta_re = re.compile('(' + '|'.join(
        re.escape(char) for char in list(meta_chars)) + ')')
    escaped_flags = re.subn(meta_re, r'\\\1', flags)[0]

    # test output tags
    self.test_complete_tag = 'TEST-{time}'.format(time=time.time())
    self.test_success_tag = 'succeeded'
    self.test_failure_tag = 'failed'

    # test command setup
    test_base_command = raspi_test_path + ' ' + escaped_flags
    test_success_output = ' && echo {} {}'.format(self.test_complete_tag,
                                                  self.test_success_tag)
    test_failure_output = ' || echo {} {}'.format(self.test_complete_tag,
                                                  self.test_failure_tag)
    self.test_command = '{} {} {}'.format(
        test_base_command, test_success_output, test_failure_output)

  def _PexpectSpawnAndConnect(self, command):
    """Spawns a process with pexpect and connect to the raspi.

    Args:
       command: The command to use when spawning the pexpect process.
    """

    logging.info('executing: %s', command)
    self.pexpect_process = pexpect.spawn(
        command, timeout=Launcher._PEXPECT_TIMEOUT)
    retry_count = 0
    expected_prompts = [
        r'.*Are\syou\ssure.*',  # Fingerprint verification
        r'.* password:',  # Password prompt
        '.*[a-zA-Z]+.*',  # Any other text input
    ]
    while True:
      try:
        i = self.pexpect_process.expect(expected_prompts)
        if i == 0:
          self.pexpect_process.sendline('yes')
        elif i == 1:
          self.pexpect_process.sendline(Launcher._RASPI_PASSWORD)
          break
        else:
          # If any other input comes in, maybe we've logged in with rsa key or
          # raspi does not have password. Check if we've logged in by echoing
          # a special sentence and expect it back.
          self.pexpect_process.sendline('echo ' + Launcher._SSH_LOGIN_SIGNAL)
          i = self.pexpect_process.expect([Launcher._SSH_LOGIN_SIGNAL])
          break
      except pexpect.TIMEOUT:
        if self.shutdown_initiated.is_set():
          return
        retry_count += 1
        # Check if the max retry count has been exceeded. If it has, then
        # re-raise the timeout exception.
        if retry_count > Launcher._PEXPECT_PASSWORD_TIMEOUT_MAX_RETRIES:
          exc_info = sys.exc_info()
          raise exc_info[0], exc_info[1], exc_info[2]

  def _PexpectReadLines(self):
    """Reads all lines from the pexpect process."""

    retry_count = 0
    while True:
      try:
        # Sanitize the line to remove ansi color codes.
        line = Launcher._PEXPECT_SANITIZE_LINE_RE.sub(
            '', self.pexpect_process.readline())
        if not line:
          break
        self.output_file.write(line)
        self.output_file.flush()
        # Check for the test complete tag. It will be followed by either a
        # success or failure tag.
        if line.startswith(self.test_complete_tag):
          if line.find(self.test_success_tag) != -1:
            self.return_value = 0
          break
        # A line was successfully read without timing out; reset the retry
        # count before attempting to read the next line.
        retry_count = 0
      except pexpect.TIMEOUT:
        if self.shutdown_initiated.is_set():
          return
        retry_count += 1
        # Check if the max retry count has been exceeded. If it has, then
        # re-raise the timeout exception.
        if retry_count > Launcher._PEXPECT_READLINE_TIMEOUT_MAX_RETRIES:
          exc_info = sys.exc_info()
          raise exc_info[0], exc_info[1], exc_info[2]

  def _CleanupPexpectProcess(self):
    """Closes current pexpect process."""

    if self.pexpect_process is not None and self.pexpect_process.isalive():
      # Send ctrl-c to the raspi and close the process.
      self.pexpect_process.sendline(chr(3))
      self._KillExistingCobaltProcesses()
      self.pexpect_process.close()

  def _KillExistingCobaltProcesses(self):
    """If there are leftover Cobalt processes, kill them.

    It is possible that a previous process did not exit cleanly.
    Zombie Cobalt instances can block the WebDriver port or
    cause other problems.
    """
    self.pexpect_process.sendline('pkill -9 -f "(cobalt)|(crashpad_handler)"')
    # Print the return code of pkill. 0 if a process was halted
    self.pexpect_process.sendline('echo $?')
    i = self.pexpect_process.expect([r'0', r'.*'])
    if i == '0':
      logging.warning(
          'Forced to pkill existing instance(s) of cobalt. '
          'Pausing to ensure no further operations are run '
          'before processes shut down.')
      time.sleep(10)

  def Run(self):
    """Runs launcher's executable on the target raspi.

    Returns:
       Whether or not the run finished successfully.
    """

    self.return_value = 1

    try:
      # Notify other threads that the run is now active
      self.run_inactive.clear()

      # rsync the test files to the raspi
      if not self.shutdown_initiated.is_set():
        self._PexpectSpawnAndConnect(self.rsync_command)
      if not self.shutdown_initiated.is_set():
        self._PexpectReadLines()

      # ssh into the raspi and run the test
      if not self.shutdown_initiated.is_set():
        self._PexpectSpawnAndConnect(self.ssh_command)
      if not self.shutdown_initiated.is_set():
        self._KillExistingCobaltProcesses()
        self.pexpect_process.sendline(self.test_command)
        self._PexpectReadLines()

    except pexpect.EOF:
      logging.exception('pexpect encountered EOF while reading line.')
    except pexpect.TIMEOUT:
      logging.exception('pexpect timed out while reading line.')
    except Exception:  # pylint: disable=broad-except
      logging.exception('Error occured while running test.')
    finally:
      self._CleanupPexpectProcess()

      # Notify other threads that the run is no longer active
      self.run_inactive.set()

    return self.return_value

  def Kill(self):
    """Stops the run so that the launcher can be killed."""

    sys.stderr.write('\n***Killing Launcher***\n')
    if self.run_inactive.is_set():
      return
    # Initiate the shutdown. This causes the run to abort within one second.
    self.shutdown_initiated.set()
    # Wait up to three seconds for the run to be set to inactive.
    self.run_inactive.wait(3)

  def GetDeviceIp(self):
    """Gets the device IP."""
    return self.device_id
