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

# pylint: disable=C6204
try:
  # This code works for Python 2.
  import urlparse
  from urllib import urlencode
except ImportError:
  # This code works for Python 3.
  import urllib.parse as urlparse
  from urllib.parse import urlencode

# This directory is a package
sys.path.insert(0, os.path.abspath("."))
# pylint: disable=C6204,C6203
import c_val_names
import container_util
import tv
import tv_testcase_runner
import tv_testcase_util

# selenium imports
# pylint: disable=C0103
WebDriverWait = tv_testcase_util.import_selenium_module(
    submodule="webdriver.support.ui").WebDriverWait

ElementNotVisibleException = tv_testcase_util.import_selenium_module(
    submodule="common.exceptions").ElementNotVisibleException

BASE_URL = "https://www.youtube.com/"
TV_APP_PATH = "/tv"
BASE_PARAMS = {"env_forcedOffAllExperiments": True}
WINDOWDRIVER_CREATED_TIMEOUT_SECONDS = 30
PAGE_LOAD_WAIT_SECONDS = 30
PROCESSING_TIMEOUT_SECONDS = 15
MEDIA_TIMEOUT_SECONDS = 30


class TvTestCase(unittest.TestCase):
  """Base class for WebDriver tests.

  Style note: snake_case is used for function names here so as to match
  with an internal class with the same name.
  """

  class WindowDriverCreatedTimeoutException(BaseException):
    """Exception thrown when WindowDriver was not created in time."""

  class ProcessingTimeoutException(BaseException):
    """Exception thrown when processing did not complete in time."""

  class MediaTimeoutException(BaseException):
    """Exception thrown when media did not complete in time."""

  @classmethod
  def setUpClass(cls):
    print("Running " + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    print("Done " + cls.__name__)

  def setUp(self):
    self.wait_for_windowdriver_created()

  def get_webdriver(self):
    return tv_testcase_runner.GetWebDriver()

  def get_windowdriver_created(self):
    return tv_testcase_runner.GetWindowDriverCreated()

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

  def goto(self, path, query_params=None):
    """Goes to a path off of BASE_URL.

    Args:
      path: URL path without the hostname.
      query_params: Dictionary of parameter names and values.
    Raises:
      Underlying WebDriver exceptions
    """
    parsed_url = list(urlparse.urlparse(BASE_URL))
    parsed_url[2] = path
    query_dict = BASE_PARAMS.copy()
    if query_params:
      query_dict.update(urlparse.parse_qsl(parsed_url[4]))
      container_util.merge_dict(query_dict, query_params)
    parsed_url[4] = urlencode(query_dict, doseq=True)
    final_url = urlparse.urlunparse(parsed_url)
    self.get_windowdriver_created().clear()
    self.get_webdriver().get(final_url)
    self.wait_for_windowdriver_created()

  def load_tv(self, label=None, additional_query_params=None):
    """Loads the main TV page and waits for it to display.

    Args:
      label: A value for the label query parameter.
      additional_query_params: A dict containing additional query parameters.
    Raises:
      Underlying WebDriver exceptions
    """
    query_params = {}
    if label is not None:
      query_params = {"label": label}
    if additional_query_params is not None:
      query_params.update(additional_query_params)
    self.goto(TV_APP_PATH, query_params)
    # Note that the internal tests use "expect_transition" which is
    # a mechanism that sets a maximum timeout for a "@with_retries"
    # decorator-driven success retry loop for subsequent webdriver requests.
    #
    # We'll skip that sophistication here.
    self.poll_until_found(tv.FOCUSED_SHELF)

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

  def send_keys(self, css_selector, keys):
    """Sends keys to an element uniquely identified by a selector.

    This method retries for a timeout period if the selected element
    could not be immediately found. If the retries do not succeed,
    the underlying exception is passed through.

    Args:
      css_selector: A CSS selector
      keys: key events

    Raises:
      Underlying WebDriver exceptions
    """
    start_time = time.time()
    while True:
      try:
        element = self.unique_find(css_selector)
        element.send_keys(keys)
        return
      except ElementNotVisibleException:
        # TODO ElementNotVisibleException seems to be considered
        # a "falsy" exception in the internal tests, which seems to mean
        # it would not be retried. But here, it seems essential.
        if time.time() - start_time >= PAGE_LOAD_WAIT_SECONDS:
          raise
        time.sleep(1)

  def wait_for_windowdriver_created(self):
    """Waits for Cobalt to create a WindowDriver."""
    windowdriver_created = self.get_windowdriver_created()
    if not windowdriver_created.wait(WINDOWDRIVER_CREATED_TIMEOUT_SECONDS):
      raise TvTestCase.WindowDriverCreatedTimeoutException()

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
    return (self.get_cval(c_val_names.count_dom_active_dispatch_events()) or
            self.get_cval(c_val_names.layout_is_dirty()) or
            (check_animations and
             self.get_cval(c_val_names.renderer_has_active_animations())) or
            self.get_cval(c_val_names.count_image_cache_loading_resources()))

  def wait_for_media_element_playing(self):
    """Waits for a video to begin playing.

    Raises:
      MediaTimeoutException: The video does not start playing within the
      required time.
    """
    start_time = time.time()
    while self.get_cval(
        c_val_names.event_duration_dom_video_start_delay()) == 0:
      if time.time() - start_time > MEDIA_TIMEOUT_SECONDS:
        raise TvTestCase.MediaTimeoutException()

      time.sleep(0.1)


def main():
  logging.basicConfig(level=logging.DEBUG)
  tv_testcase_runner.main()
