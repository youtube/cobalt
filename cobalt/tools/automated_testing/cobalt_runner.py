"""Launches Cobalt and runs webdriver-based Cobalt tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import os
import re
import sys
import thread
import threading
import time

import _env  # pylint: disable=unused-import
from cobalt.tools.automated_testing import c_val_names
from cobalt.tools.automated_testing import webdriver_utils
from starboard.tools import abstract_launcher
from starboard.tools import command_line

# Pattern to match Cobalt log line for when the WebDriver port has been
# opened.
RE_WEBDRIVER_LISTEN = re.compile(r'Starting WebDriver server on port (\d+)')
# Pattern to match Cobalt log line for when a WindowDriver has been created.
RE_WINDOWDRIVER_CREATED = re.compile(
    r'^\[\d+/\d+:INFO:browser_module\.cc\(\d+\)\] Created WindowDriver: ID=\S+')
# Pattern to match Cobalt log line for when a WebModule is has been loaded.
RE_WEBMODULE_LOADED = re.compile(
    r'^\[\d+/\d+:INFO:browser_module\.cc\(\d+\)\] Loaded WebModule')

# selenium imports
# pylint: disable=C0103
ActionChains = webdriver_utils.import_selenium_module(
    submodule='webdriver.common.action_chains').ActionChains
keys = webdriver_utils.import_selenium_module('webdriver.common.keys')

DEFAULT_STARTUP_TIMEOUT_SECONDS = 2 * 60
WEBDRIVER_HTTP_TIMEOUT_SECONDS = 2 * 60
COBALT_EXIT_TIMEOUT_SECONDS = 5
PAGE_LOAD_WAIT_SECONDS = 30
WINDOWDRIVER_CREATED_TIMEOUT_SECONDS = 30
WEBMODULE_LOADED_TIMEOUT_SECONDS = 30

COBALT_WEBDRIVER_CAPABILITIES = {
    'browserName': 'cobalt',
    'javascriptEnabled': True,
    'platform': 'LINUX'
}


class TimeoutException(Exception):
  """General timeout exception."""
  pass


class CobaltRunner(object):
  """Wrapper around a Starboard applauncher object specialized to launch Cobalt.

  In addition to launching Cobalt, this class also includes logic to attach (via
  Selenium) a webdriver client to the Cobalt process, and wait for some common
  events to occur such as when the initial URL finishes loading.  Additional
  functionality is provided for common operations such as querying from the
  Cobalt process the value of a CVal.
  """

  class DeviceParams(object):
    """A struct to store runner's device info."""
    platform = None
    device_id = None
    config = None
    out_directory = None

  class WindowDriverCreatedTimeoutException(Exception):
    """Exception thrown when WindowDriver was not created in time."""

  class WebModuleLoadedTimeoutException(Exception):
    """Exception thrown when WebModule was not loaded in time."""

  class AssertException(Exception):
    """Raised when assert condition fails."""

  test_script_started = threading.Event()
  selenium_webdriver_module = None
  launcher = None
  webdriver = None
  thread = None
  failed = False
  should_exit = threading.Event()
  launcher_is_running = False
  windowdriver_created = threading.Event()
  webmodule_loaded = threading.Event()

  def __init__(self,
               device_params,
               url,
               log_file=None,
               target_params=None,
               success_message=None):
    """CobaltRunner constructor.

    Args:
      device_params:    A DeviceParams object storing all device specific info.
      url:              The intial URL to launch Cobalt on.
      log_file:         The log file's name string.
      target_params:    An array of command line arguments to launch Cobalt
      with.
      success_message:  Optional success message to be printed on successful
      exit.
    """

    self.selenium_webdriver_module = webdriver_utils.import_selenium_module(
        'webdriver')

    self.platform = device_params.platform
    self.config = device_params.config
    self.device_id = device_params.device_id
    self.out_directory = device_params.out_directory
    if log_file:
      self.log_file = open(log_file)
    else:
      self.log_file = sys.stdout
    self.url = url
    self.target_params = target_params
    self.success_message = success_message
    self.target_params.append('--url=' + self.url)

  def SendResume(self):
    """Sends a resume signal to start Cobalt from preload."""
    self.launcher.SendResume()

  def SendSuspend(self):
    """Sends a system signal to put Cobalt into suspend state."""
    self.launcher.SendSuspend()

  def GetURL(self):
    return self.url

  def GetLogFile(self):
    return self.log_file

  def GetWindowDriverCreated(self):
    """Returns the WindowDriver created instance."""
    return self.windowdriver_created

  def GetWebModuleLoaded(self):
    """Returns the WebModule loaded instance."""
    return self.webmodule_loaded

  def _HandleLine(self):
    """Reads log lines to determine when cobalt/webdriver server start."""
    while True:
      line = self.launcher_read_pipe.readline()
      if line:
        self.log_file.write(line)
      else:
        break

      if RE_WINDOWDRIVER_CREATED.search(line):
        self.windowdriver_created.set()
        continue

      if RE_WEBMODULE_LOADED.search(line):
        self.webmodule_loaded.set()
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

    self.Run()
    return self

  def Run(self):
    """Construct and run app launcher."""
    if self.launcher_is_running:
      return

    # Behavior to restart a killed app launcher is not clearly defined.
    # Let's get a new launcher for every launch.
    read_fd, write_fd = os.pipe()

    self.launcher_read_pipe = os.fdopen(read_fd, 'r')
    self.launcher_write_pipe = os.fdopen(write_fd, 'w')

    self.launcher = abstract_launcher.LauncherFactory(
        self.platform,
        'cobalt',
        self.config,
        device_id=self.device_id,
        target_params=self.target_params,
        output_file=self.launcher_write_pipe,
        out_directory=self.out_directory)

    self.runner_thread = threading.Thread(target=self._RunLauncher)
    self.runner_thread.start()

    self.reader_thread = threading.Thread(target=self._HandleLine)
    # Make this thread daemonic so that it always exits
    self.reader_thread.daemon = True
    self.reader_thread.start()
    self.launcher_is_running = True
    try:
      self.WaitForStart()
    except KeyboardInterrupt:
      # potentially from thread.interrupt_main(). We will treat as
      # a timeout regardless

      self.Exit(should_fail=True)
      raise TimeoutException

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

    self.KillLauncher()

  def KillLauncher(self):
    """Kills the launcher and its attached Cobalt instance."""
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
    host, webdriver_port = self.launcher.GetHostAndPortGivenPort(port)
    url = 'http://{}:{}/'.format(host, webdriver_port)
    self.webdriver = self.selenium_webdriver_module.Remote(
        url, COBALT_WEBDRIVER_CAPABILITIES)
    self.webdriver.command_executor.set_timeout(WEBDRIVER_HTTP_TIMEOUT_SECONDS)
    print('Selenium Connected\n', file=self.log_file)
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

  def _RunLauncher(self):
    """Thread run routine."""
    try:
      print('Running launcher', file=self.log_file)
      self.launcher.Run()
      print(
          'Cobalt terminated. failed: ' + str(self.failed), file=self.log_file)
      if not self.failed and self.success_message:
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

  def GetCval(self, cval_name):
    """Returns the Python object represented by a JSON cval string.

    Args:
      cval_name: Name of the cval.
    Returns:
      Python object represented by the JSON cval string
    """
    javascript_code = 'return h5vcc.cVal.getValue(\'{}\')'.format(cval_name)
    json_result = self.webdriver.execute_script(javascript_code)
    if json_result is None:
      return None
    else:
      return json.loads(json_result)

  def PollUntilFound(self, css_selector, expected_num=None):
    """Polls until an element is found.

    Args:
      css_selector: A CSS selector
      expected_num: The expected number of the selector type to be found.
    Raises:
      Underlying WebDriver exceptions
    """
    start_time = time.time()
    while ((not self.FindElements(css_selector)) and
           (time.time() - start_time < PAGE_LOAD_WAIT_SECONDS)):
      time.sleep(1)
    if expected_num:
      self.FindElements(css_selector, expected_num)

  def UniqueFind(self, unique_selector):
    """Finds and returns a uniquely selected element.

    Args:
      unique_selector: A CSS selector that will select only one element
    Raises:
      AssertException: the element isn't unique
    Returns:
      Element
    """
    return self.FindElements(unique_selector, expected_num=1)[0]

  def AssertDisplayed(self, css_selector):
    """Asserts that an element is displayed.

    Args:
      css_selector: A CSS selector
    Raises:
      AssertException: the element isn't found
    """
    # TODO does not actually assert that it's visible, like webdriver.py
    # probably does.
    if not self.UniqueFind(css_selector):
      raise CobaltRunner.AssertException(
          'Did not find selector: {}'.format(css_selector))

  def FindElements(self, css_selector, expected_num=None):
    """Finds elements based on a selector.

    Args:
      css_selector: A CSS selector
      expected_num: Expected number of matching elements
    Raises:
      AssertException: expected_num isn't met
    Returns:
      Array of selected elements
    """
    elements = self.webdriver.find_elements_by_css_selector(css_selector)
    if expected_num is not None and len(elements) != expected_num:
      raise CobaltRunner.AssertException(
          'Expected number of element {} is: {}, got {}'.format(
              css_selector, expected_num, len(elements)))
    return elements

  def SendKeys(self, key_events):
    """Sends keys to whichever element currently has focus.

    Args:
      key_events: key events

    Raises:
      Underlying WebDriver exceptions
    """
    ActionChains(self.webdriver).send_keys(key_events).perform()

  def ClearUrlLoadedEvents(self):
    """Clear the events that indicate that Cobalt finished loading a URL."""

    self.GetWindowDriverCreated().clear()
    self.GetWebModuleLoaded().clear()

  def WaitForUrlLoadedEvents(self):
    """Wait for the events indicating that Cobalt finished loading a URL."""
    if not self.windowdriver_created.wait(WINDOWDRIVER_CREATED_TIMEOUT_SECONDS):
      raise CobaltRunner.WindowDriverCreatedTimeoutException()

    if not self.webmodule_loaded.wait(WEBMODULE_LOADED_TIMEOUT_SECONDS):
      raise CobaltRunner.WebModuleLoadedTimeoutException()

  def LoadUrl(self, url):
    """Loads about:blank and waits for it to finish.

    Args:
      url:  URL string to be loaded by Cobalt.
    Raises:
      Underlying WebDriver exceptions
    """
    self.ClearUrlLoadedEvents()
    self.webdriver.get(url)
    self.WaitForUrlLoadedEvents()

  def IsInPreload(self):
    """We assume Cobalt is in preload mode if no render tree is generated."""
    render_tree_count = self.GetCval(
        c_val_names.count_rasterize_new_render_tree())
    if render_tree_count is not None:
      return False
    return True


def GetDeviceParamsFromCommandLine():
  """Provide a commanline parser for all CobaltRunner inputs."""

  arg_parser = command_line.CreateParser()

  args, _ = arg_parser.parse_known_args()

  device_params = CobaltRunner.DeviceParams()
  device_params.platform = args.platform
  device_params.config = args.config
  device_params.device_id = args.device_id
  device_params.out_directory = args.out_directory

  return device_params
