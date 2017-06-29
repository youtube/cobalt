"""Base class for WebDriver tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import logging
import os
import sys
import time
import unittest

# This directory is a package
sys.path.insert(0, os.path.abspath("."))
# pylint: disable=C6204,C6203
import c_val_names
import tv
import tv_testcase_runner
import tv_testcase_util

# selenium imports
# pylint: disable=C0103
ActionChains = tv_testcase_util.import_selenium_module(
    submodule="webdriver.common.action_chains").ActionChains
keys = tv_testcase_util.import_selenium_module("webdriver.common.keys")

WINDOWDRIVER_CREATED_TIMEOUT_SECONDS = 30
WEBMODULE_LOADED_TIMEOUT_SECONDS = 30
PAGE_LOAD_WAIT_SECONDS = 30
PROCESSING_TIMEOUT_SECONDS = 60
HTML_SCRIPT_ELEMENT_EXECUTE_TIMEOUT_SECONDS = 30
MEDIA_TIMEOUT_SECONDS = 30
SKIP_AD_TIMEOUT_SECONDS = 30
TITLE_CARD_HIDDEN_TIMEOUT_SECONDS = 30

_is_initialized = False


class TvTestCase(unittest.TestCase):
  """Base class for WebDriver tests.

  Style note: snake_case is used for function names here so as to match
  with an internal class with the same name.
  """

  class WindowDriverCreatedTimeoutException(BaseException):
    """Exception thrown when WindowDriver was not created in time."""

  class WebModuleLoadedTimeoutException(BaseException):
    """Exception thrown when WebModule was not loaded in time."""

  class ProcessingTimeoutException(BaseException):
    """Exception thrown when processing did not complete in time."""

  class HtmlScriptElementExecuteTimeoutException(BaseException):
    """Exception thrown when processing did not complete in time."""

  class TitleCardHiddenTimeoutException(BaseException):
    """Exception thrown when title card did not disappear in time."""

  @classmethod
  def setUpClass(cls):
    print("Running " + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    print("Done " + cls.__name__)

  def setUp(self):
    global _is_initialized
    if not _is_initialized:
      # Initialize the tests. This involves loading a URL which applies the
      # forcedOffAllExperiments cookies, ensuring that no subsequent loads
      # include experiments. Additionally, loading this URL triggers a reload.
      query_params = {"env_forcedOffAllExperiments": True}
      triggers_reload = True
      self.load_tv(query_params, triggers_reload)
      _is_initialized = True

  def get_webdriver(self):
    return tv_testcase_runner.GetWebDriver()

  def get_default_url(self):
    return tv_testcase_runner.GetDefaultUrl()

  def get_cval(self, cval_name):
    """Returns the Python object represented by a JSON cval string.

    Args:
      cval_name: Name of the cval.
    Returns:
      Python object represented by the JSON cval string
    """
    javascript_code = "return h5vcc.cVal.getValue('{}')".format(cval_name)
    json_result = self.get_webdriver().execute_script(javascript_code)
    if json_result is None:
      return None
    else:
      return json.loads(json_result)

  def load_blank(self):
    """Loads about:blank and waits for it to finish.

    Raises:
      Underlying WebDriver exceptions
    """
    self.clear_url_loaded_events()
    self.get_webdriver().get("about:blank")
    self.wait_for_url_loaded_events()

  def load_tv(self, query_params=None, triggers_reload=False):
    """Loads the main TV page and waits for it to display.

    Args:
      query_params: A dict containing additional query parameters.
      triggers_reload: Whether or not the navigation will trigger a reload.
    Raises:
      Underlying WebDriver exceptions
    """
    self.get_webdriver().execute_script("h5vcc.storage.clearCookies()")
    self.clear_url_loaded_events()
    self.get_webdriver().get(
        tv_testcase_util.generate_url(self.get_default_url(), query_params))
    self.wait_for_url_loaded_events()
    if triggers_reload:
      self.clear_url_loaded_events()
      self.wait_for_url_loaded_events()
    # Note that the internal tests use "expect_transition" which is
    # a mechanism that sets a maximum timeout for a "@with_retries"
    # decorator-driven success retry loop for subsequent webdriver requests.
    #
    # We'll skip that sophistication here.
    self.wait_for_processing_complete_after_focused_shelf()

  def poll_until_found(self, css_selector):
    """Polls until an element is found.

    Args:
      css_selector: A CSS selector
    Raises:
      Underlying WebDriver exceptions
    """
    start_time = time.time()
    while ((not self.find_elements(css_selector)) and
           (time.time() - start_time < PAGE_LOAD_WAIT_SECONDS)):
      time.sleep(1)
    self.assert_displayed(css_selector)

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
    tv_testcase_runner.GetWindowDriverCreated().clear()
    tv_testcase_runner.GetWebModuleLoaded().clear()

  def wait_for_url_loaded_events(self):
    """Wait for the events indicating that Cobalt finished loading a URL."""
    windowdriver_created = tv_testcase_runner.GetWindowDriverCreated()
    if not windowdriver_created.wait(WINDOWDRIVER_CREATED_TIMEOUT_SECONDS):
      raise TvTestCase.WindowDriverCreatedTimeoutException()

    webmodule_loaded = tv_testcase_runner.GetWebModuleLoaded()
    if not webmodule_loaded.wait(WEBMODULE_LOADED_TIMEOUT_SECONDS):
      raise TvTestCase.WebModuleLoadedTimeoutException()

  def wait_for_processing_complete_after_focused_shelf(self):
    """Waits for Cobalt to focus on a shelf and complete pending layouts."""
    self.poll_until_found(tv.FOCUSED_SHELF)
    self.assert_displayed(tv.FOCUSED_SHELF_TITLE)
    self.wait_for_processing_complete()

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
        raise TvTestCase.ProcessingTimeoutException()

      time.sleep(0.1)

    # Now wait for all processing to complete in Cobalt.
    count = 0
    while count < 2:
      if self.is_processing(check_animations):
        count = 0
      else:
        count += 1

      if time.time() - start_time > PROCESSING_TIMEOUT_SECONDS:
        raise TvTestCase.ProcessingTimeoutException()

      time.sleep(0.1)

  def is_processing(self, check_animations):
    """Checks to see if Cobalt is currently processing."""
    return (self.get_cval(c_val_names.count_dom_active_java_script_events()) or
            self.get_cval(c_val_names.layout_is_dirty()) or
            (check_animations and
             self.get_cval(c_val_names.renderer_has_active_animations())) or
            self.get_cval(c_val_names.count_image_cache_loading_resources()))

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
        raise TvTestCase.HtmlScriptElementExecuteTimeoutException()
      time.sleep(0.1)

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

  def skip_advertisement_if_playing(self):
    """Waits to skip an ad if it is encountered.

    Returns:
      True if a skippable advertisement was encountered
    """
    start_time = time.time()
    if not self.find_elements(tv.SKIP_AD_BUTTON_HIDDEN):
      while not self.find_elements(tv.SKIP_AD_BUTTON_CAN_SKIP):
        if time.time() - start_time > SKIP_AD_TIMEOUT_SECONDS:
          return True
        time.sleep(0.1)
      self.send_keys(keys.Keys.ENTER)
      self.wait_for_processing_complete(False)
      return True

    return False

  def wait_for_title_card_hidden(self):
    """Waits for the title to disappear while a video is playing.

    Raises:
      TitleCardHiddenTimeoutException: The title card did not become hidden in
      the required time.
    """
    start_time = time.time()
    while not self.find_elements(tv.TITLE_CARD_HIDDEN):
      if time.time() - start_time > TITLE_CARD_HIDDEN_TIMEOUT_SECONDS:
        raise TvTestCase.TitleCardHiddenTimeoutException()
      time.sleep(1)


def main():
  logging.basicConfig(level=logging.DEBUG)
  tv_testcase_runner.main()
