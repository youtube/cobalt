"""Launches Cobalt and runs webdriver-based Cobalt tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import logging
import os
import re
import sys
import threading
import time
import traceback
import thread

import _env  # pylint: disable=unused-import
from cobalt.tools.automated_testing import c_val_names
from cobalt.tools.automated_testing import webdriver_utils
from starboard.tools import abstract_launcher

# Pattern to match Cobalt log line for when the WebDriver port has been
# opened.
RE_WEBDRIVER_LISTEN = re.compile(r'Starting WebDriver server on port (\d+)')
# Pattern to match Cobalt log line if WebDriver server fails to start.
RE_WEBDRIVER_FAILED = re.compile(r'Could not start WebDriver server')
# Pattern to match Cobalt log line for when a WindowDriver has been created.
RE_WINDOWDRIVER_CREATED = re.compile(
    (r'^\[[\d:]+/[\d.]+:INFO:browser_module\.cc\(\d+\)\] Created WindowDriver: '
     r'ID=\S+'))
# Pattern to match Cobalt log line for when a WebModule is has been loaded.
RE_WEBMODULE_LOADED = re.compile(
    r'^\[[\d:]+/[\d.]+:INFO:browser_module\.cc\(\d+\)\] Loaded WebModule')

# selenium imports
# pylint: disable=C0103
ActionChains = webdriver_utils.import_selenium_module(
    submodule='webdriver.common.action_chains').ActionChains
keys = webdriver_utils.import_selenium_module('webdriver.common.keys')
selenium_exceptions = webdriver_utils.import_selenium_module(
    'common.exceptions')

DEFAULT_STARTUP_TIMEOUT_SECONDS = 2 * 60
WEBDRIVER_HTTP_TIMEOUT_SECONDS = 2 * 60
COBALT_EXIT_TIMEOUT_SECONDS = 5
PAGE_LOAD_WAIT_SECONDS = 30
WINDOWDRIVER_CREATED_TIMEOUT_SECONDS = 45
WEBMODULE_LOADED_TIMEOUT_SECONDS = 45
FIND_ELEMENT_RETRY_LIMIT = 20
EXECUTE_JAVASCRIPT_RETRY_LIMIT = 10

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

  class WindowDriverCreatedTimeoutException(Exception):
    """Exception thrown when WindowDriver was not created in time."""

  class WebModuleLoadedTimeoutException(Exception):
    """Exception thrown when WebModule was not loaded in time."""

  class AssertException(Exception):
    """Raised when assert condition fails."""

  def __init__(self,
               launcher_params,
               url,
               log_file=None,
               target_params=None,
               success_message=None,
               log_handler=None):
    """CobaltRunner constructor.

    Args:
      launcher_params:  An object storing all platform configuration and device
        information parameters.
      url:              The initial URL to launch Cobalt on.
      log_file:         The log file's name string.
      target_params:    An array of command line arguments to launch Cobalt
        with.
      success_message:  Optional success message to be printed on successful
        exit.
    """

    self.test_script_started = threading.Event()
    self.launcher = None
    self.webdriver = None
    self.failed = False
    self.should_exit = threading.Event()
    self.launcher_is_running = False
    self.windowdriver_created = threading.Event()
    self.webmodule_loaded = threading.Event()

    self.selenium_webdriver_module = webdriver_utils.import_selenium_module(
        'webdriver')

    self.launcher_params = launcher_params
    self.log_handler = log_handler

    if log_file:
      self.log_file = open(log_file)
      logging.basicConfig(stream=self.log_file, level=logging.INFO)
    else:
      self.log_file = sys.stdout
    self.url = url
    self.target_params = target_params
    self.success_message = success_message
    url_string = '--url=' + self.url
    if not self.target_params:
      self.target_params = [url_string]
    else:
      self.target_params.append(url_string)
    if self.launcher_params.target_params:
      self.target_params.extend(self.launcher_params.target_params)

  def SendResume(self):
    """Sends a resume signal to start Cobalt from preload."""
    self.launcher.SendResume()

  def SendSuspend(self):
    """Sends a system signal to put Cobalt into suspend state."""
    self.launcher.SendSuspend()

  def SendConceal(self):
    """Sends a system signal to put Cobalt into concealed state."""
    self.launcher.SendConceal()

  def SendFocus(self):
    """Sends a system signal to put Cobalt into started state."""
    self.launcher.SendFocus()

  def SendFreeze(self):
    """Sends a system signal to put Cobalt into frozen state."""
    self.launcher.SendFreeze()

  def SendStop(self):
    """Sends a system signal to put Cobalt into stopped state."""
    self.launcher.SendStop()

  def SendSystemSuspend(self):
    """Ask the system to suspend Cobalt."""
    self.launcher.SendSystemSuspend()

  def SendSystemResume(self):
    """Ask the system to resume Cobalt."""
    self.launcher.SendSystemResume()

  def SendDeepLink(self, link):
    """Sends a deep link to Cobalt."""
    return self.launcher.SendDeepLink(link)

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
        if self.log_handler is not None:
          self.log_handler(line)
        self.log_file.write(line)
        # Calling flush() to ensure the logs are delivered timely.
        self.log_file.flush()
      else:
        break

      if not self.windowdriver_created.set() and \
          RE_WINDOWDRIVER_CREATED.search(line):
        self.windowdriver_created.set()
        continue

      if not self.webmodule_loaded.set() and \
          RE_WEBMODULE_LOADED.search(line):
        self.webmodule_loaded.set()
        continue

      # Wait for WebDriver port here then connect
      if self.test_script_started.is_set():
        continue

      # Bail out immediately if the Cobalt WebDriver server doesn't start.
      if RE_WEBDRIVER_FAILED.search(line):
        logging.error('\nCobalt WebDriver server not started.'
                      '\nIs another instance of Cobalt running?')
        self.launcher.Kill()

      match = RE_WEBDRIVER_LISTEN.search(line)
      if not match:
        continue

      port = match.group(1)
      logging.info('WebDriver port opened:' + port + '\n')
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
        self.launcher_params.platform,
        'cobalt',
        self.launcher_params.config,
        device_id=self.launcher_params.device_id,
        target_params=self.target_params,
        output_file=self.launcher_write_pipe,
        out_directory=self.launcher_params.out_directory,
        loader_platform=self.launcher_params.loader_platform,
        loader_config=self.launcher_params.loader_config,
        loader_out_directory=self.launcher_params.loader_out_directory)

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
      # a timeout regardless.

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

    self._KillLauncher()

  def _KillLauncher(self):
    """Kills the launcher and its attached Cobalt instance."""
    wait_for_runner_thread = True
    if self.CanExecuteJavaScript():
      try:
        self.ExecuteJavaScript('window.close();')
      except Exception:
        wait_for_runner_thread = False
        sys.stderr.write(
            '***An exception was raised while trying to close the app:')
        traceback.print_exc(file=sys.stderr)

    if wait_for_runner_thread:
      self.runner_thread.join(COBALT_EXIT_TIMEOUT_SECONDS)
    if self.runner_thread.isAlive():
      sys.stderr.write(
          '***Runner thread still alive after sending graceful shutdown '
          'command, try again by killing app***\n')
      self.launcher.Kill()
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
    logging.info('Selenium Connected')
    self.test_script_started.set()

  def WaitForStart(self):
    """Waits for the webdriver client to attach to Cobalt."""
    startup_timeout_seconds = self.launcher.GetStartupTimeout()
    if not startup_timeout_seconds:
      startup_timeout_seconds = DEFAULT_STARTUP_TIMEOUT_SECONDS

    if not self.test_script_started.wait(startup_timeout_seconds):
      self.Exit(should_fail=True)
      raise TimeoutException
    logging.info('Cobalt started')

  def _RunLauncher(self):
    """Thread run routine."""
    try:
      logging.info('Running launcher')
      self.launcher.Run()
      logging.info('Cobalt terminated.')
      if not self.failed and self.success_message:
        print('{}\n'.format(self.success_message))
        logging.info('%s\n', self.success_message)
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

  def CanExecuteJavaScript(self):
    return self.webdriver is not None

  def ExecuteJavaScript(self, js_code):
    retry_count = 0
    while True:
      if retry_count >= EXECUTE_JAVASCRIPT_RETRY_LIMIT:
        raise TimeoutException(
            'Selenium element or window not found in {} tries'.format(
                EXECUTE_JAVASCRIPT_RETRY_LIMIT))
      retry_count += 1
      try:
        result = self.webdriver.execute_script(js_code)
      except (selenium_exceptions.NoSuchElementException,
              selenium_exceptions.NoSuchWindowException):
        time.sleep(0.2)
        continue
      break
    return result

  def GetCval(self, cval_name):
    """Returns the Python object represented by a cval string.

    Args:
      cval_name: Name of the cval.

    Returns:
      Python object represented by the cval string
    """
    javascript_code = 'return h5vcc.cVal.getValue(\'{}\')'.format(cval_name)
    cval_string = self.ExecuteJavaScript(javascript_code)
    if cval_string:
      try:
        # Try to parse numbers and booleans.
        return json.loads(cval_string)
      except ValueError:
        # If we can't parse a value, return the cval string as-is.
        return cval_string
    return None

  def GetCvalBatch(self, cval_name_list):
    """Retrieves a batch of cvals.

    Use this instead of retrieving individual cvals to reduce the overhead of
    the query. There can be several milliseconds of latency for each individual
    query.

    Args:
      cval_name_list: List of cval names.

    Returns:
      Python dictionary of values indexed by the cval names provided.
    """
    javascript_code_list = [
        'h5vcc.cVal.getValue(\'{}\')'.format(name) for name in cval_name_list
    ]
    javascript_code = 'return [' + ','.join(javascript_code_list) + ']'
    json_results = self.ExecuteJavaScript(javascript_code)
    cval_value_list = [
        None if result is None else json.loads(result)
        for result in json_results
    ]
    return dict(zip(cval_name_list, cval_value_list))

  def GetUserAgent(self):
    """Returns the User Agent string."""
    return self.ExecuteJavaScript('return navigator.userAgent;')

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
    # The retry part below is a temporary workaround to handle command
    # failures during a short window of stale Cobalt WindowDriver
    # after navigation. We only introduced it because of limited time budget
    # at the moment, please don't introduce any code that relies on it.
    retry_count = 0
    while True:
      if retry_count >= FIND_ELEMENT_RETRY_LIMIT:
        raise TimeoutException(
            'Selenium element or window not found in {} tries'.format(
                FIND_ELEMENT_RETRY_LIMIT))
      retry_count += 1
      try:
        elements = self.webdriver.find_elements_by_css_selector(css_selector)
      except (selenium_exceptions.NoSuchElementException,
              selenium_exceptions.NoSuchWindowException):
        time.sleep(0.2)
        continue
      break
    if expected_num and len(elements) != expected_num:
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
