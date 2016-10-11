#!/usr/bin/python
"""Webdriver-based benchmarks for partial layout.

Benchmarks for partial layout changes in Cobalt.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import importlib
import os
import re
import socket
import sys
import threading

arg_parser = argparse.ArgumentParser(
    description="Runs Webdriver-based Cobalt benchmarks")
arg_parser.add_argument(
    "-p", "--platform",
    help="Cobalt platform, eg 'linux-x64x11'."
    "Fetched from environment if absent.")
arg_parser.add_argument(
    "-e", "--executable",
    help="Path to cobalt executable. "
    "Auto-derived if absent.")
arg_parser.add_argument(
    "-c", "--config",
    choices=["debug", "devel", "qa", "gold"],
    help="Build config (eg, 'qa' or 'devel'). Not used if "
    "--executable is specified. Fetched from environment "
    "if needed and absent.")
arg_parser.add_argument(
    "-d", "--devkit_name",
    help="Devkit or IP address for app_launcher."
    "Current hostname used if absent.")
arg_parser.add_argument(
    "-o", "--log_file", help="Logfile pathname. stdout if absent.")

# Pattern to match Cobalt log line for when the WebDriver port has been
# opened.
RE_WEBDRIVER_LISTEN = re.compile(r"Starting WebDriver server on port (\d+)$")

COBALT_WEBDRIVER_CAPABILITIES = {
    "browserName": "cobalt",
    "javascriptEnabled": True,
    "platform": "LINUX"
}


class CobaltRunner(object):
  """Runs a Cobalt browser w/ a WebDriver client attached."""
  test_script_started = threading.Event()
  should_exit = threading.Event()
  selenium_webdriver_module = None
  webdriver = None
  launcher = None
  log_file_path = None
  thread = None

  def __init__(self, platform, executable, devkit_name, log_file_path):
    # As of this writing, Google uses selenium 3.0.0b2 internally, so
    # thats what we will target here as well.
    try:
      self.selenium_webdriver_module = importlib.import_module(
          "selenium.webdriver")
      if not self.selenium_webdriver_module.__version__.startswith("3.0"):
        raise ImportError("Not version 3.0.x")
    except ImportError:
      sys.stderr.write("Please install selenium >= 3.0.0b2.\n"
                       "Commonly: \"sudo pip install 'selenium>=3.0.0b2'\"\n")
      sys.exit(1)

    script_path = os.path.dirname(__file__)
    sys.path.append(script_path + "/../../tools/lbshell/")
    app_launcher = importlib.import_module("app_launcher")
    self.launcher = app_launcher.CreateLauncher(
        platform, executable, devkit_name=devkit_name)

    self.launcher.SetArgs(["--enable_webdriver"])
    self.launcher.SetOutputCallback(self._HandleLine)
    self.log_file_path = log_file_path

  def __enter__(self):
    self.thread = threading.Thread(target=self.Run)
    self.thread.start()
    self.WaitForStart()

  def __exit__(self, exc_type, exc_value, traceback):
    self.SetShouldExit()
    self.thread.join()

  def _HandleLine(self, line):
    """Internal log line callback."""

    done = self.should_exit.is_set()
    # Wait for WebDriver port here then connect
    if self.test_script_started.is_set():
      return done

    match = RE_WEBDRIVER_LISTEN.search(line)
    if not match:
      return done

    port = match.group(1)
    self._StartWebdriver(port)
    return done

  def SetShouldExit(self):
    """Indicates cobalt process should exit. Done at next log line output."""
    self.should_exit.set()

  def _GetIPAddress(self):
    return self.launcher.GetIPAddress()

  def _StartWebdriver(self, port):
    url = "http://{}:{}/".format(self._GetIPAddress(), port)
    self.webdriver = self.selenium_webdriver_module.Remote(
        url, COBALT_WEBDRIVER_CAPABILITIES)
    self.test_script_started.set()

  def WaitForStart(self):
    """Waits for the webdriver client to attach to cobalt."""
    self.test_script_started.wait()

  def Run(self):
    """Thread run routine."""

    # Use stdout if log_file_path is unspecified
    # If log_file_path is specified, make sure to close it
    to_close = None
    try:
      if self.log_file_path:
        log_file = open(self.log_file_path, "w")
        to_close = log_file
      else:
        log_file = sys.stdout

      self.launcher.SetOutputFile(log_file)
      self.launcher.Run()
    finally:
      if to_close:
        to_close.close()
    return 0


def GetCobaltExecutablePath(platform, config):
  """Auto-derives a path to a cobalt executable."""
  if config is None:
    try:
      config = os.environ["BUILD_TYPE"]
    except KeyError:
      sys.stderr.write("Must specify --config or --executable\n")
      sys.exit(1)
  script_dir = os.path.dirname(os.path.realpath(__file__))
  out_dir = os.path.join(script_dir, "..", "..", "out")
  executable_directory = os.path.join(out_dir, "{}_{}".format(platform, config))
  return os.path.join(executable_directory, "cobalt")


def main():
  args = arg_parser.parse_args()

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

  devkit_name = args.devkit_name
  if devkit_name is None:
    devkit_name = socket.gethostname()

  with CobaltRunner(platform, executable, devkit_name, args.log_file):
    # TODO run tests here
    pass
  return 0


if __name__ == "__main__":
  sys.exit(main())
