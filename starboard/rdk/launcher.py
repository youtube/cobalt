#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""RDK implementation of Starboard launcher abstraction."""

import functools
import logging
import os
import re
import signal
import six
import sys
import threading
import time
import contextlib

import pexpect
from starboard.tools import abstract_launcher
from starboard.shared import retry


# pylint: disable=unused-argument
def _sigint_or_sigterm_handler(signum, frame):
  """Clean up and exit with status |signum|.

  Args:
    signum: Signal number that triggered this callback.  Passed in when the
      signal handler is called by python runtime.
    frame: Current stack frame.  Passed in when the signal handler is called by
      python runtime.
  """
  sys.exit(signum)


# First call returns True, otherwise return false.
def first_run():
  v = globals()
  if 'first_run' not in v:
    v['first_run'] = False
    return True
  return False


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt/tools on RDK."""

  _STARTUP_TIMEOUT_SECONDS = 1800

  _RDK_USERNAME = 'root'
  _RDK_PASSWORD = ''
  _RDK_PROMPT = 'root@AmlogicFirebolt:'
  _RDK_LOG_FILE = '/opt/logs/wpeframework.log'

  _SSH_LOGIN_SIGNAL = 'cobalt-launcher-login-success'
  _SSH_SLEEP_SIGNAL = 'cobalt-launcher-done-sleeping'

  # pexpect times out each second to allow Kill to quickly stop a test run
  _PEXPECT_TIMEOUT = 1

  # SSH shell command retries
  _PEXPECT_SPAWN_RETRIES = 20

  # pexpect.sendline retries
  _PEXPECT_SENDLINE_RETRIES = 3

  # Old process kill retries
  _KILL_RETRIES = 3

  _PEXPECT_SHUTDOWN_SLEEP_TIME = 3
  # Time to wait after processes were killed
  _PROCESS_KILL_SLEEP_TIME = 10

  # Retrys for getting a clean prompt
  _PROMPT_WAIT_MAX_RETRIES = 5
  # Wait up to 10 seconds for the password prompt from the RDK
  _PEXPECT_PASSWORD_TIMEOUT_MAX_RETRIES = 10
  # Wait up to 900 seconds for new output from the RDK
  _PEXPECT_READLINE_TIMEOUT_MAX_RETRIES = 900
  # Delay between subsequent SSH commands
  _INTER_COMMAND_DELAY_SECONDS = 1.5

  # This is used to strip ansi color codes from pexpect output.
  _PEXPECT_SANITIZE_LINE_RE = re.compile(r'\x1b[^m]*m')

  # Exceptions to retry
  _RETRY_EXCEPTIONS = (pexpect.TIMEOUT, pexpect.ExceptionPexpect,
                       pexpect.exceptions.EOF, OSError)

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    # pylint: disable=super-with-arguments
    super().__init__(platform, target_name, config, device_id, **kwargs)
    env = os.environ.copy()
    env.update(self.env_variables)
    self.full_env = env
    self.platform = platform
    self.device_id = device_id

    if not self.device_id:
      self.device_id = self.full_env.get('RDK_ADDR')
      if not self.device_id:
        raise ValueError(
            'Unable to determine target, please pass it in, or set RDK_ADDR '
            'environment variable.')

    self.startup_timeout_seconds = Launcher._STARTUP_TIMEOUT_SECONDS

    self.pexpect_process = None

    self._InitPexpectCommands()

    self.run_inactive = threading.Event()
    self.run_inactive.set()

    self.shutdown_initiated = threading.Event()

    self.log_targets = kwargs.get('log_targets', True)

    signal.signal(signal.SIGINT, functools.partial(_sigint_or_sigterm_handler))
    signal.signal(signal.SIGTERM, functools.partial(_sigint_or_sigterm_handler))

    self.last_run_pexpect_cmd = ''

  def _InitPexpectCommands(self):
    """Initializes all of the pexpect commands needed for running the test."""

    # Ensure no trailing slashes
    self.out_directory = self.out_directory.rstrip('/')

    rdk_user_hostname = f'{Launcher._RDK_USERNAME}@{self.device_id}'
    rdk_test_dir = '/usr/share/content/data/app'

    # RDK has limited storage space left on the partition /usr and may
    # not be able to fit the entire test target with content folder.
    # Therefore the test data will be uploaded to /data partition and create
    # symlink under /usr/share/content/data/app for loader to use.
    rdk_storage_dir = '/data/cobalt_test_data'

    # rsync command setup
    options = '-avzLhc'
    source = os.path.join(self.out_directory, 'content', 'app',
                          self.target_name)
    destination = f'{rdk_user_hostname}:{rdk_storage_dir}/'
    self.rsync_command = 'rsync ' + options + ' ' + source + ' ' + destination

    # ssh command setup
    rsa_options = (
        '-o \"LogLevel ERROR\" '
        '-o \"UserKnownHostsFile=/dev/null\" -o \"StrictHostKeyChecking=no\"')
    self.ssh_command = (f'ssh -t {rsa_options} {rdk_user_hostname} '
                        f'TERM=dumb bash -l')

    # test file preparation
    # The very last 'cobalt' in the path is to meet the hard coded path
    # used in RDK's loader
    rdk_tmp = '/var/lib/persistent/rdkservices/Cobalt/Cobalt/.cobalt_storage'
    self.test_prep_command = (f'rm -rf {rdk_tmp}; '
                              f'rm -rf {rdk_test_dir}/cobalt; '
                              f'ln -s {rdk_storage_dir}/{self.target_name} '
                              f'{rdk_test_dir}/cobalt')

    # test output tags
    self.test_complete_tag_1 = 'test suite ran.'
    self.test_complete_tag_2 = 'test suites ran.'
    self.test_failure_tag = 'tests, listed below'
    self.test_success_tag = 'succeeded'

    def _request_payload(method):
      """Create the request paylaod needed for running the test."""

      cmd_test_param = (f'"sbmainargs":{self.target_command_line_params}'
                        if self.target_command_line_params else '')
      json_cmd = (
          f'\'{{"jsonrpc": "2.0","id": 3,"method": "org.rdk.RDKShell.{method}",'
          f'"params": {{"callsign":"YouTube","type":"Cobalt",'
          f'"configuration":{{ {cmd_test_param}}} }} }}\' ')
      return json_cmd

    # test command setup
    cmd_log = f'tail -f {Launcher._RDK_LOG_FILE}'
    self.test_command = (f'curl -X POST http://127.0.0.1:9998/jsonrpc -d '
                         f'{_request_payload("1.launch")}; '
                         f'{cmd_log}')
    self.terminate_command = (f'curl -X POST http://127.0.0.1:9998/jsonrpc -d '
                              f'{_request_payload("destroy")}; ')

  # pylint: disable=no-method-argument
  def _CommandBackoff():
    time.sleep(Launcher._INTER_COMMAND_DELAY_SECONDS)

  def _ShutdownBackoff(self):
    Launcher._CommandBackoff()
    return self.shutdown_initiated.is_set()

  @retry.retry(
      exceptions=_RETRY_EXCEPTIONS,
      retries=_PEXPECT_SPAWN_RETRIES,
      backoff=_CommandBackoff)
  def _PexpectSpawnAndConnect(self, command):
    """Spawns a process with pexpect and connect to the RDK.

    Args:
       command: The command to use when spawning the pexpect process.
    """

    logging.info('executing: %s', command)
    kwargs = {} if six.PY2 else {'encoding': 'utf-8'}
    self.pexpect_process = pexpect.spawn(
        command, timeout=Launcher._PEXPECT_TIMEOUT, **kwargs)
    # Let pexpect output directly to our output stream
    self.pexpect_process.logfile_read = self.output_file
    expected_prompts = [
        r'.*Are\syou\ssure.*',  # Fingerprint verification
        r'.* password:',  # Password prompt
        '.*[a-zA-Z]+.*',  # Any other text input
    ]

    # pylint: disable=unnecessary-lambda
    @retry.retry(
        exceptions=Launcher._RETRY_EXCEPTIONS,
        retries=Launcher._PEXPECT_PASSWORD_TIMEOUT_MAX_RETRIES,
        backoff=lambda: self._ShutdownBackoff(),
        wrap_exceptions=False)
    def _inner():
      i = self.pexpect_process.expect(expected_prompts)
      if i == 0:
        self._PexpectSendLine('yes')
      elif i == 1:
        self._PexpectSendLine(Launcher._RDK_PASSWORD)
      else:
        # If any other input comes in, maybe we've logged in with rsa key or
        # RDK does not have password. Check if we've logged in by echoing
        # a special sentence and expect it back.
        self._PexpectSendLine('echo ' + Launcher._SSH_LOGIN_SIGNAL)
        i = self.pexpect_process.expect([Launcher._SSH_LOGIN_SIGNAL])

    _inner()

  @retry.retry(
      exceptions=_RETRY_EXCEPTIONS,
      retries=_PEXPECT_SENDLINE_RETRIES,
      wrap_exceptions=False)
  def _PexpectSendLine(self, cmd):
    """Send lines to Pexpect and record the last command for logging purposes"""
    logging.info('sending >> : %s ', cmd)
    self.last_run_pexpect_cmd = cmd
    self.pexpect_process.sendline(cmd)

  def _PexpectReadLines(self):
    """Reads all lines from the pexpect process."""
    # pylint: disable=unnecessary-lambda
    @retry.retry(
        exceptions=Launcher._RETRY_EXCEPTIONS,
        retries=Launcher._PEXPECT_READLINE_TIMEOUT_MAX_RETRIES,
        backoff=lambda: self.shutdown_initiated.is_set(),
        wrap_exceptions=False)
    def _readloop():
      while True:
        # Sanitize the line to remove ansi color codes.
        line = Launcher._PEXPECT_SANITIZE_LINE_RE.sub(
            '', self.pexpect_process.readline())
        self.output_file.flush()
        if not line:
          return
        # Check for the test complete tag. It will be followed by either a
        # success or failure tag.
        if (line.find(self.test_complete_tag_1) != -1 or
            line.find(self.test_complete_tag_2) != -1):
          self.return_value = 0
          return

    _readloop()

  def _Sleep(self, val):
    self._PexpectSendLine(f'sleep {val};echo {Launcher._SSH_SLEEP_SIGNAL}')
    self.pexpect_process.expect([Launcher._SSH_SLEEP_SIGNAL])

  def _CleanupPexpectProcess(self):
    """Closes current pexpect process."""

    if self.pexpect_process is not None and self.pexpect_process.isalive():
      # Check if kernel logged OOM kill or any other system failure message
      if self.return_value:
        logging.info('Sending dmesg')
        with contextlib.suppress(Launcher._RETRY_EXCEPTIONS):
          self._PexpectSendLine('dmesg -P --color=never | tail -n 100')
        time.sleep(self._PEXPECT_SHUTDOWN_SLEEP_TIME)
        with contextlib.suppress(Launcher._RETRY_EXCEPTIONS):
          self.pexpect_process.readlines()
        logging.info('Done sending dmesg')

      # Send ctrl-c to the RDK and close the process.
      with contextlib.suppress(Launcher._RETRY_EXCEPTIONS):
        self._PexpectSendLine(chr(3))
      time.sleep(self._PEXPECT_TIMEOUT)  # Allow time for normal shutdown
      with contextlib.suppress(Launcher._RETRY_EXCEPTIONS):
        self.pexpect_process.close()

  def _WaitForPrompt(self):
    """Sends empty commands, until a bash prompt is returned"""

    def backoff():
      self._PexpectSendLine('echo ' + Launcher._SSH_SLEEP_SIGNAL)
      return self._ShutdownBackoff()

    retry.with_retry(
        lambda: self.pexpect_process.expect(self._RDK_PROMPT),
        exceptions=Launcher._RETRY_EXCEPTIONS,
        retries=Launcher._PROMPT_WAIT_MAX_RETRIES,
        backoff=backoff,
        wrap_exceptions=False)

  @retry.retry(
      exceptions=_RETRY_EXCEPTIONS,
      retries=_KILL_RETRIES,
      backoff=_CommandBackoff)
  def _KillExistingCobaltProcesses(self):
    """If there are leftover Cobalt processes, kill them.

    It is possible that a previous process did not exit cleanly.
    Zombie Cobalt instances can block the WebDriver port or
    cause other problems.
    """
    logging.info('Killing existing processes')
    self._PexpectSendLine(self.terminate_command)
    self._WaitForPrompt()
    logging.info('Done killing existing processes')

  def Run(self):
    """Runs launcher's executable on the target RDK.

    Returns:
       Whether or not the run finished successfully.
    """

    if self.log_targets:
      logging.info('-' * 32)
      logging.info('Starting to run target: %s', self.target_name)
      logging.info('=' * 32)

    self.return_value = 1

    try:
      # Notify other threads that the run is now active
      self.run_inactive.clear()

      # copy the test files to the RDK
      if not self.shutdown_initiated.is_set():
        self._PexpectSpawnAndConnect(self.rsync_command)

      if not self.shutdown_initiated.is_set():
        self._PexpectReadLines()

      # ssh into the RDK and run the test
      if not self.shutdown_initiated.is_set():
        self._PexpectSpawnAndConnect(self.ssh_command)
        self._Sleep(self._INTER_COMMAND_DELAY_SECONDS)

      # Execute debugging commands on the first run
      first_run_commands = []
      if self.test_result_xml_path:
        first_run_commands.append(f'touch {self.test_result_xml_path}')

      first_run_commands.extend(['free -mh', 'ps -ux', 'df -h'])
      if first_run():
        for cmd in first_run_commands:
          if not self.shutdown_initiated.is_set():
            self._PexpectSendLine(cmd)

            def _readline():
              line = self.pexpect_process.readline()
              self.output_file.write(line)

            retry.with_retry(
                _readline,
                exceptions=Launcher._RETRY_EXCEPTIONS,
                retries=Launcher._PROMPT_WAIT_MAX_RETRIES)
        self._WaitForPrompt()
        self.output_file.flush()
        self._Sleep(self._INTER_COMMAND_DELAY_SECONDS)
        self._KillExistingCobaltProcesses()
        self._Sleep(self._INTER_COMMAND_DELAY_SECONDS)

      if not self.shutdown_initiated.is_set():
        self._PexpectSendLine(self.test_prep_command)
        self._Sleep(self._INTER_COMMAND_DELAY_SECONDS)
        self._PexpectSendLine(self.test_command)
        self._PexpectReadLines()

    except retry.RetriesExceeded:
      logging.exception('Command retry exceeded (cmd: %s)',
                        self.last_run_pexpect_cmd)
    except pexpect.EOF:
      logging.exception('pexpect encountered EOF while reading line. (cmd: %s)',
                        self.last_run_pexpect_cmd)
    except pexpect.TIMEOUT:
      logging.exception('pexpect timed out while reading line. (cmd: %s)',
                        self.last_run_pexpect_cmd)
    except Exception:  # pylint: disable=broad-except
      logging.exception('Error occurred while running test. (cmd: %s)',
                        self.last_run_pexpect_cmd)
    finally:
      self._CleanupPexpectProcess()

      # Notify other threads that the run is no longer active
      self.run_inactive.set()

    if self.log_targets:
      logging.info('-' * 32)
      logging.info('Finished running target: %s', self.target_name)
      logging.info('=' * 32)

    return self.return_value

  def Kill(self):
    """Stops the run so that the launcher can be killed."""

    sys.stderr.write('\n***Killing Launcher***\n')
    if self.run_inactive.is_set():
      return
    # Initiate the shutdown. This causes the run to abort within one second.
    self.shutdown_initiated.set()
    # Wait up to three seconds for the run to be set to inactive.
    self.run_inactive.wait(Launcher._PEXPECT_SHUTDOWN_SLEEP_TIME)

  def GetDeviceIp(self):
    """Gets the device IP."""
    return self.device_id

  def GetDeviceOutputPath(self):
    """Writable path where test targets can output files"""
    return '/tmp'
