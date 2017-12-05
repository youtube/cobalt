"""Launches Cobalt and runs webdriver-based Cobalt tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import re
import sys
import thread
import threading

import _env  # pylint: disable=unused-import
from starboard.tools import abstract_launcher

# pylint: disable=C6204
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import webdriver_utils

# Pattern to match Cobalt log line for when the WebDriver port has been
# opened.
RE_WEBDRIVER_LISTEN = re.compile(r'Starting WebDriver server on port (\d+)')
# Pattern to match Cobalt log line for when a WindowDriver has been created.
RE_WINDOWDRIVER_CREATED = re.compile(
    r'^\[\d+/\d+:INFO:browser_module\.cc\(\d+\)\] Created WindowDriver: ID=\S+')
# Pattern to match Cobalt log line for when a WebModule is has been loaded.
RE_WEBMODULE_LOADED = re.compile(
    r'^\[\d+/\d+:INFO:browser_module\.cc\(\d+\)\] Loaded WebModule')

DEFAULT_STARTUP_TIMEOUT_SECONDS = 2 * 60
WEBDRIVER_HTTP_TIMEOUT_SECONDS = 2 * 60
COBALT_EXIT_TIMEOUT_SECONDS = 5

COBALT_WEBDRIVER_CAPABILITIES = {
    'browserName': 'cobalt',
    'javascriptEnabled': True,
    'platform': 'LINUX'
}

_launcher = None
_webdriver = None
_windowdriver_created = threading.Event()
_webmodule_loaded = threading.Event()


def SendResume():
  """Sends a resume signal to start Cobalt from preload."""
  _launcher.SendResume()


def SendSuspend():
  """Sends a system signal to put Cobalt into suspend state."""
  _launcher.SendSuspend()


def GetWebDriver():
  """Returns the active connect WebDriver instance."""
  return _webdriver


def GetWindowDriverCreated():
  """Returns the WindowDriver created instance."""
  return _windowdriver_created


def GetWebModuleLoaded():
  """Returns the WebModule loaded instance."""
  return _webmodule_loaded


class TimeoutException(Exception):
  pass


class CobaltRunner(object):
  """Runs a Cobalt browser w/ a WebDriver client attached."""
  test_script_started = threading.Event()
  selenium_webdriver_module = None
  launcher = None
  webdriver = None
  thread = None
  failed = False
  should_exit = threading.Event()

  def __init__(self, platform, config, device_id, target_params, out_directory,
               url, log_file, success_message):
    global _launcher

    self.selenium_webdriver_module = webdriver_utils.import_selenium_module(
        'webdriver')

    command_line_args = []
    if target_params is not None:
      split_target_params = target_params.split(' ')
      for param in split_target_params:
        command_line_args.append(param)
    command_line_args.append('--debug_console=off')
    command_line_args.append('--null_savegame')
    command_line_args.append('--url=' + url)

    self.log_file = log_file
    self.success_message = success_message

    read_fd, write_fd = os.pipe()

    self.launcher_read_pipe = os.fdopen(read_fd, 'r')
    self.launcher_write_pipe = os.fdopen(write_fd, 'w')

    self.launcher = abstract_launcher.LauncherFactory(
        platform,
        'cobalt',
        config,
        device_id=device_id,
        target_params=command_line_args,
        output_file=self.launcher_write_pipe,
        out_directory=out_directory)
    _launcher = self.launcher

  def _HandleLine(self):
    """Reads log lines to determine when cobalt/webdriver server start."""
    while True:
      line = self.launcher_read_pipe.readline()
      if line:
        self.log_file.write(line)
      else:
        break

      if RE_WINDOWDRIVER_CREATED.search(line):
        _windowdriver_created.set()
        continue

      if RE_WEBMODULE_LOADED.search(line):
        _webmodule_loaded.set()
        continue

      # Wait for WebDriver port here then connect
      if self.test_script_started.is_set():
        continue

      match = RE_WEBDRIVER_LISTEN.search(line)
      if not match:
        continue

      port = match.group(1)
      print('WebDriver port opened:' + port + '\n', file=self.log_file)
      self._StartWebdriver(port)

  def __enter__(self):

    self.runner_thread = threading.Thread(target=self.Run)
    self.runner_thread.start()

    self.reader_thread = threading.Thread(target=self._HandleLine)
    # Make this thread daemonic so that it always exits
    self.reader_thread.daemon = True
    self.reader_thread.start()
    try:
      self.WaitForStart()
    except KeyboardInterrupt:
      # potentially from thread.interrupt_main(). We will treat as
      # a timeout regardless

      self.Exit(should_fail=True)
      raise TimeoutException
    return self

  def __exit__(self, exc_type, exc_value, exc_traceback):
    # The unittest module terminates with a SystemExit
    # If this is a successful exit, then this is a successful run
    success = exc_type is None or (exc_type is SystemExit and
                                   not exc_value.code)
    self.Exit(should_fail=not success)

  def Exit(self, should_fail=False):
    if not self.should_exit.is_set():
      self._SetShouldExit(failed=should_fail)

  def _SetShouldExit(self, failed=False):
    """Indicates Cobalt process should exit."""
    self.failed = failed
    self.should_exit.set()

    try:
      self.launcher.Kill()
    except Exception as e:  # pylint: disable=broad-except
      sys.stderr.write('Exception killing launcher:\n')
      sys.stderr.write('{}\n'.format(str(e)))

    self.runner_thread.join(COBALT_EXIT_TIMEOUT_SECONDS)
    if self.runner_thread.isAlive():
      sys.stderr.write('***Runner thread still alive***\n')
    # Once the write end of the pipe has been closed by the launcher, the reader
    # thread will get EOF and exit.
    self.reader_thread.join(COBALT_EXIT_TIMEOUT_SECONDS)
    if self.reader_thread.isAlive():
      sys.stderr.write('***Reader thread still alive, exiting anyway***\n')
    try:
      self.launcher_read_pipe.close()
    except IOError:
      # Ignore error from closing the pipe during a blocking read
      pass

  def _StartWebdriver(self, port):
    global _webdriver
    host, webdriver_port = self.launcher.GetHostAndPortGivenPort(port)
    url = 'http://{}:{}/'.format(host, webdriver_port)
    self.webdriver = self.selenium_webdriver_module.Remote(
        url, COBALT_WEBDRIVER_CAPABILITIES)
    self.webdriver.command_executor.set_timeout(WEBDRIVER_HTTP_TIMEOUT_SECONDS)
    print('Selenium Connected\n', file=self.log_file)
    _webdriver = self.webdriver
    self.test_script_started.set()

  def WaitForStart(self):
    """Waits for the webdriver client to attach to Cobalt."""
    startup_timeout_seconds = self.launcher.GetStartupTimeout()
    if not startup_timeout_seconds:
      startup_timeout_seconds = DEFAULT_STARTUP_TIMEOUT_SECONDS

    if not self.test_script_started.wait(startup_timeout_seconds):
      self.Exit(should_fail=True)
      raise TimeoutException
    print('Cobalt started', file=self.log_file)

  def Run(self):
    """Thread run routine."""
    try:
      print('Running launcher', file=self.log_file)
      self.launcher.Run()
      print(
          'Cobalt terminated. failed: ' + str(self.failed), file=self.log_file)
      if not self.failed:
        print('{}\n'.format(self.success_message))
    # pylint: disable=broad-except
    except Exception as ex:
      sys.stderr.write('Exception running Cobalt ' + str(ex))
    finally:
      self.launcher_write_pipe.close()
      if not self.should_exit.is_set():
        # If the main thread is not expecting us to exit,
        # we must interrupt it.
        thread.interrupt_main()
    return 0
