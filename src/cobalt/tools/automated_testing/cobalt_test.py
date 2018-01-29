"""Base class for tests that need to launch Cobalt."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import os
import sys
import time
import traceback
import unittest

# pylint: disable=C6204
import _env  # pylint: disable=unused-import
from cobalt.tools.automated_testing import c_val_names
from cobalt.tools.automated_testing import cobalt_runner
from cobalt.tools.automated_testing import webdriver_utils
from starboard.tools import command_line

# selenium imports
# pylint: disable=C0103
ActionChains = webdriver_utils.import_selenium_module(
    submodule='webdriver.common.action_chains').ActionChains
keys = webdriver_utils.import_selenium_module('webdriver.common.keys')

WINDOWDRIVER_CREATED_TIMEOUT_SECONDS = 30
WEBMODULE_LOADED_TIMEOUT_SECONDS = 30
PAGE_LOAD_WAIT_SECONDS = 30
PROCESSING_TIMEOUT_SECONDS = 180
HTML_SCRIPT_ELEMENT_EXECUTE_TIMEOUT_SECONDS = 30
MEDIA_TIMEOUT_SECONDS = 30
DEFAULT_TEST_SUCCESS_MESSAGE = 'Cobalt Test Case passed.'
DEFAULT_URL = 'https://www.youtube.com/tv'

_platform = 'unknown'


class CobaltTestCase(unittest.TestCase):

  class WindowDriverCreatedTimeoutException(Exception):
    """Exception thrown when WindowDriver was not created in time."""

  class WebModuleLoadedTimeoutException(Exception):
    """Exception thrown when WebModule was not loaded in time."""

  class ProcessingTimeoutException(Exception):
    """Exception thrown when processing did not complete in time."""

  class HtmlScriptElementExecuteTimeoutException(Exception):
    """Exception thrown when processing did not complete in time."""

  def setUp(self):
    pass

  @classmethod
  def setUpClass(cls):
    print('Running ' + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    print('Done ' + cls.__name__)

  def get_platform(self):
    return _platform

  def get_webdriver(self):
    return cobalt_runner.GetWebDriver()

  def send_resume(self):
    return cobalt_runner.SendResume()

  def send_suspend(self):
    return cobalt_runner.SendSuspend()

  def get_cval(self, cval_name):
    """Returns the Python object represented by a JSON cval string.

    Args:
      cval_name: Name of the cval.
    Returns:
      Python object represented by the JSON cval string
    """
    javascript_code = 'return h5vcc.cVal.getValue(\'{}\')'.format(cval_name)
    json_result = self.get_webdriver().execute_script(javascript_code)
    if json_result is None:
      return None
    else:
      return json.loads(json_result)

  def poll_until_found(self, css_selector, expected_num=None):
    """Polls until an element is found.

    Args:
      css_selector: A CSS selector
      expected_num: The expected number of the selector type to be found.
    Raises:
      Underlying WebDriver exceptions
    """
    start_time = time.time()
    while ((not self.find_elements(css_selector)) and
           (time.time() - start_time < PAGE_LOAD_WAIT_SECONDS)):
      time.sleep(1)
    if expected_num:
      self.find_elements(css_selector, expected_num)

  def unique_find(self, unique_selector):
    """Finds and returns a uniquely selected element.

    Args:
      unique_selector: A CSS selector that will select only one element
    Raises:
      Underlying WebDriver exceptions
      AssertError: the element isn't unique
    Returns:
      Element
    """
    return self.find_elements(unique_selector, expected_num=1)[0]

  def assert_displayed(self, css_selector):
    """Asserts that an element is displayed.

    Args:
      css_selector: A CSS selector
    Raises:
      Underlying WebDriver exceptions
      AssertError: the element isn't found
    """
    # TODO does not actually assert that it's visible, like webdriver.py
    # probably does.
    self.assertTrue(self.unique_find(css_selector))

  def find_elements(self, css_selector, expected_num=None):
    """Finds elements based on a selector.

    Args:
      css_selector: A CSS selector
      expected_num: Expected number of matching elements
    Raises:
      Underlying WebDriver exceptions
      AssertError: expected_num isn't met
    Returns:
      Array of selected elements
    """
    elements = self.get_webdriver().find_elements_by_css_selector(css_selector)
    if expected_num is not None:
      self.assertEqual(len(elements), expected_num)
    return elements

  def send_keys(self, key_events):
    """Sends keys to whichever element currently has focus.

    Args:
      key_events: key events

    Raises:
      Underlying WebDriver exceptions
    """
    ActionChains(self.get_webdriver()).send_keys(key_events).perform()

  def clear_url_loaded_events(self):
    """Clear the events that indicate that Cobalt finished loading a URL."""
    cobalt_runner.GetWindowDriverCreated().clear()
    cobalt_runner.GetWebModuleLoaded().clear()

  def load_blank(self):
    """Loads about:blank and waits for it to finish.

    Raises:
      Underlying WebDriver exceptions
    """
    self.clear_url_loaded_events()
    self.get_webdriver().get('about:blank')
    self.wait_for_url_loaded_events()

  def wait_for_url_loaded_events(self):
    """Wait for the events indicating that Cobalt finished loading a URL."""
    windowdriver_created = cobalt_runner.GetWindowDriverCreated()
    if not windowdriver_created.wait(WINDOWDRIVER_CREATED_TIMEOUT_SECONDS):
      raise CobaltTestCase.WindowDriverCreatedTimeoutException()

    webmodule_loaded = cobalt_runner.GetWebModuleLoaded()
    if not webmodule_loaded.wait(WEBMODULE_LOADED_TIMEOUT_SECONDS):
      raise CobaltTestCase.WebModuleLoadedTimeoutException()

  def wait_for_processing_complete(self, check_animations=True):
    """Waits for Cobalt to complete processing.

    This method requires two consecutive iterations through its loop where
    Cobalt is not processing before treating processing as complete. This
    protects against a brief window between two different processing sections
    being mistaken as completed processing.

    Args:
      check_animations: Whether or not animations should be checked when
                        determining if processing is complete.

    Raises:
      ProcessingTimeoutException: Processing is not complete within the
      required time.
    """
    start_time = time.time()

    # First simply check for whether or not the event is still processing.
    # There's no need to check anything else while the event is still going on.
    # Once it is done processing, it won't get re-set, so there's no need to
    # re-check it.
    while self.get_cval(c_val_names.event_is_processing()):
      if time.time() - start_time > PROCESSING_TIMEOUT_SECONDS:
        raise CobaltTestCase.ProcessingTimeoutException()

      time.sleep(0.1)

    # Now wait for all processing to complete in Cobalt.
    count = 0
    while count < 2:
      if self.is_processing(check_animations):
        count = 0
      else:
        count += 1

      if time.time() - start_time > PROCESSING_TIMEOUT_SECONDS:
        raise CobaltTestCase.ProcessingTimeoutException()

      time.sleep(0.1)

  def is_processing(self, check_animations):
    """Checks to see if Cobalt is currently processing."""
    return (self.get_cval(c_val_names.count_dom_active_java_script_events()) or
            self.get_cval(c_val_names.layout_is_render_tree_pending()) or
            (check_animations and
             self.get_cval(c_val_names.renderer_has_active_animations())) or
            self.get_cval(c_val_names.count_image_cache_resource_loading()))

  def wait_for_media_element_playing(self):
    """Waits for a video to begin playing.

    Returns:
      Whether or not the video started.
    """
    start_time = time.time()
    while self.get_cval(
        c_val_names.event_duration_dom_video_start_delay()) == 0:
      if time.time() - start_time > MEDIA_TIMEOUT_SECONDS:
        return False
      time.sleep(0.1)

    return True

  def wait_for_html_script_element_execute_count(self, required_count):
    """Waits for specified number of html script element Execute() calls.

    Args:
      required_count: the number of executions that must occur

    Raises:
      HtmlScriptElementExecuteTimeoutException: The required html script element
      executions did not occur within the required time.
    """
    start_time = time.time()
    while self.get_cval(
        c_val_names.count_dom_html_script_element_execute()) < required_count:
      if time.time() - start_time > HTML_SCRIPT_ELEMENT_EXECUTE_TIMEOUT_SECONDS:
        raise CobaltTestCase.HtmlScriptElementExecuteTimeoutException()
      time.sleep(0.1)

  def is_in_preload(self):
    """We assume Cobalt is in preload mode if no render tree is generated."""
    render_tree_count = self.get_cval(
        c_val_names.count_rasterize_new_render_tree())
    if render_tree_count is not None:
      return False
    return True


def main(url):
  """Launch Cobalt and run python unit test main function."""
  global _platform

  arg_parser = command_line.CreateParser()
  arg_parser.add_argument(
      '-l', '--log_file', help='Logfile pathname. stdout if absent.')
  arg_parser.add_argument(
      '--success_message', help='Custom message to be printed on test success.')

  args, _ = arg_parser.parse_known_args()
  # Keep unittest module from seeing these args
  sys.argv = sys.argv[:1]

  success_message = DEFAULT_TEST_SUCCESS_MESSAGE
  if args.success_message is not None:
    success_message = args.success_message

  _platform = args.platform
  if _platform is None:
    try:
      _platform = os.environ['BUILD_PLATFORM']
    except KeyError:
      sys.stderr.write('Must specify --platform\n')
      sys.exit(1)

  if not args.log_file:
    log_file = sys.stdout
  else:
    log_file = open(args.log_file)

  if not url:
    url = DEFAULT_URL

  try:
    with cobalt_runner.CobaltRunner(
        platform=_platform,
        config=args.config,
        device_id=args.device_id,
        target_params=args.target_params,
        out_directory=args.out_directory,
        url=url,
        log_file=log_file,
        success_message=success_message):
      unittest.main(
          testRunner=unittest.TextTestRunner(verbosity=0, stream=log_file))
  except cobalt_runner.TimeoutException:
    print('Timeout waiting for Cobalt to start', file=sys.stderr)
    sys.exit(1)
  # User pressed Ctrl + C, or an error in the launcher caused a shutdown.
  except KeyboardInterrupt:
    sys.exit(1)
  except Exception:  # pylint: disable=W0703
    sys.stderr.write('Exception while running test:\n')
    traceback.print_exc(file=sys.stderr)
    sys.exit(1)
