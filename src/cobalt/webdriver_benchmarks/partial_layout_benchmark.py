#!/usr/bin/python2
"""Webdriver-based benchmarks for partial layout.

Benchmarks for partial layout changes in Cobalt.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import importlib
import inspect
import os
import re
import sys
import thread
import threading
import unittest

arg_parser = argparse.ArgumentParser(
    description="Runs Webdriver-based Cobalt benchmarks")
arg_parser.add_argument(
    "-p",
    "--platform",
    help="Cobalt platform, eg 'linux-x64x11'."
    "Fetched from environment if absent.")
arg_parser.add_argument(
    "-e",
    "--executable",
    help="Path to cobalt executable. "
    "Auto-derived if absent.")
arg_parser.add_argument(
    "-c",
    "--config",
    choices=["debug", "devel", "qa", "gold"],
    help="Build config (eg, 'qa' or 'devel'). Not used if "
    "--executable is specified. Fetched from environment "
    "if needed and absent.")
arg_parser.add_argument(
    "-d",
    "--devkit_name",
    help="Devkit or IP address for app_launcher."
    "Current hostname used if absent.")
arg_parser.add_argument(
    "-o", "--log_file", help="Logfile pathname. stdout if absent.")

# Pattern to match Cobalt log line for when the WebDriver port has been
# opened.
RE_WEBDRIVER_LISTEN = re.compile(r"Starting WebDriver server on port (\d+)$")

STARTUP_TIMEOUT_SECONDS = 2 * 60

WEBDRIVER_HTTP_TIMEOUT_SECS = 2 * 60

COBALT_EXIT_TIMEOUT_SECONDS = 5

COBALT_WEBDRIVER_CAPABILITIES = {
    "browserName": "cobalt",
    "javascriptEnabled": True,
    "platform": "LINUX"
}

_webdriver = None


def GetWebDriver():
  """Returns the active connect WebDriver instance."""
  return _webdriver


def ImportSeleniumModule(submodule=None):
  """Dynamically imports a selenium.webdriver submodule.

  This is done because selenium 3.0 is not commonly pre-installed
  on workstations, and we want to have a friendly error message for that
  case.

  Args:
    submodule: module subpath underneath "selenium.webdriver"
  Returns:
    appropriate module
  """
  if submodule:
    module_path = ".".join(("selenium", submodule))
  else:
    module_path = "selenium"
  # As of this writing, Google uses selenium 3.0.0b2 internally, so
  # thats what we will target here as well.
  try:
    module = importlib.import_module(module_path)
    if submodule is None:
      # Only the top-level module has __version__
      if not module.__version__.startswith("3.0"):
        raise ImportError("Not version 3.0.x")
  except ImportError:
    sys.stderr.write("Could not import {}\n"
                     "Please install selenium >= 3.0.0b2.\n"
                     "Commonly: \"sudo pip install 'selenium>=3.0.0b2'\"\n"
                     .format(module_path))
    sys.exit(1)
  return module


class TimeoutException(Exception):
  pass


class CobaltRunner(object):
  """Runs a Cobalt browser w/ a WebDriver client attached."""
  test_script_started = threading.Event()
  selenium_webdriver_module = None
  webdriver = None
  launcher = None
  log_file_path = None
  thread = None
  failed = False
  should_exit = threading.Event()

  def __init__(self, platform, executable, devkit_name, log_file_path):
    self.selenium_webdriver_module = ImportSeleniumModule("webdriver")

    script_path = os.path.realpath(inspect.getsourcefile(lambda: 0))
    app_launcher_path = os.path.realpath(
        os.path.join(
            os.path.dirname(script_path), "..", "..", "tools", "lbshell"))
    sys.path.append(app_launcher_path)
    app_launcher = importlib.import_module("app_launcher")
    self.launcher = app_launcher.CreateLauncher(
        platform, executable, devkit_name=devkit_name, close_output_file=False)

    self.launcher.SetArgs(["--enable_webdriver"])
    self.launcher.SetOutputCallback(self._HandleLine)
    self.log_file_path = log_file_path
    self.log_file = None

  def __enter__(self):
    self.thread = threading.Thread(target=self.Run)
    self.thread.start()
    try:
      self.WaitForStart()
    except KeyboardInterrupt:
      # potentially from thread.interrupt_main(). We will treat as
      # a timeout regardless
      raise TimeoutException
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    # The unittest module terminates with a SystemExit
    # If this is a successful exit, then this is a successful run
    success = exc_type is None or (exc_type is SystemExit and
                                   not exc_value.code)
    self.SetShouldExit(failed=not success)
    self.thread.join(COBALT_EXIT_TIMEOUT_SECONDS)

  def _HandleLine(self, line):
    """Internal log line callback."""

    # Wait for WebDriver port here then connect
    if self.test_script_started.is_set():
      return

    match = RE_WEBDRIVER_LISTEN.search(line)
    if not match:
      return

    port = match.group(1)
    print("WebDriver port opened:" + port + "\n", file=self.log_file)
    self._StartWebdriver(port)

  def SetShouldExit(self, failed=False):
    """Indicates cobalt process should exit."""
    self.failed = failed
    self.should_exit.set()
    self.launcher.SendKill()

  def _GetIPAddress(self):
    return self.launcher.GetIPAddress()

  def _StartWebdriver(self, port):
    global _webdriver
    url = "http://{}:{}/".format(self._GetIPAddress(), port)
    self.webdriver = self.selenium_webdriver_module.Remote(
        url, COBALT_WEBDRIVER_CAPABILITIES)
    self.webdriver.command_executor.set_timeout(WEBDRIVER_HTTP_TIMEOUT_SECS)
    print("Selenium Connected\n", file=self.log_file)
    _webdriver = self.webdriver
    self.test_script_started.set()

  def WaitForStart(self):
    """Waits for the webdriver client to attach to cobalt."""
    if not self.test_script_started.wait(STARTUP_TIMEOUT_SECONDS):
      self.SetShouldExit(failed=True)
      raise TimeoutException
    print("Cobalt started", file=self.log_file)

  def Run(self):
    """Thread run routine."""

    # Use stdout if log_file_path is unspecified
    # If log_file_path is specified, make sure to close it
    to_close = None
    try:
      if self.log_file_path:
        self.log_file = open(self.log_file_path, "w")
        to_close = self.log_file
      else:
        self.log_file = sys.stdout

      self.launcher.SetOutputFile(self.log_file)
      print("Running launcher", file=self.log_file)
      self.launcher.Run()
      print(
          "Cobalt terminated. failed: " + str(self.failed), file=self.log_file)
      # This is watched for in webdriver_benchmark_test.py
      if not self.failed:
        sys.stdout.write("partial_layout_benchmark TEST COMPLETE\n")
    # pylint: disable=broad-except
    except Exception as ex:
      print("Exception running Cobalt " + str(ex), file=sys.stderr)
    finally:
      if to_close:
        to_close.close()
      if not self.should_exit.is_set():
        # If the main thread is not expecting us to exit,
        # we must interrupt it.
        thread.interrupt_main()
    return 0


def GetCobaltExecutablePath(platform, config):
  """Auto-derives a path to a cobalt executable."""
  if config is None:
    try:
      config = os.environ["BUILD_TYPE"]
    except KeyError:
      sys.stderr.write("Must specify --config or --executable\n")
      sys.exit(1)
  script_path = os.path.realpath(inspect.getsourcefile(lambda: 0))
  script_dir = os.path.dirname(script_path)
  out_dir = os.path.join(script_dir, "..", "..", "out")
  executable_directory = os.path.join(out_dir, "{}_{}".format(platform, config))
  return os.path.join(executable_directory, "cobalt")


def main():
  args = arg_parser.parse_args()
  # Keep unittest module from seeing these args
  sys.argv = sys.argv[:1]

  platform = args.platform
  if platform is None:
    try:
      platform = os.environ["BUILD_PLATFORM"]
    except KeyError:
      sys.stderr.write("Must specify --platform\n")
      sys.exit(1)

  executable = args.executable
  if executable is None:
    executable = GetCobaltExecutablePath(platform, args.config)

  try:
    with CobaltRunner(platform, executable, args.devkit_name,
                      args.log_file) as runner:
      unittest.main(testRunner=unittest.TextTestRunner(
          verbosity=0, stream=runner.log_file))
  except TimeoutException:
    print("Timeout waiting for Cobalt to start", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
  main()
