"""Base class for WebDriver tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import os
import sys
import time
import unittest
import urlparse

# This directory is a package
sys.path.insert(0, os.path.abspath("."))
# pylint: disable=C6204,C6203
import partial_layout_benchmark
import tv

# selenium imports
# pylint: disable=C0103
WebDriverWait = partial_layout_benchmark.ImportSeleniumModule(
    submodule="webdriver.support.ui").WebDriverWait

ElementNotVisibleException = (
    partial_layout_benchmark.ImportSeleniumModule(
        submodule="common.exceptions").ElementNotVisibleException)

BASE_URL = "https://www.youtube.com/tv?env_forcedOffAllExperiments=true"
PAGE_LOAD_WAIT_SECONDS = 30
LAYOUT_TIMEOUT_SECONDS = 5


class TvTestCase(unittest.TestCase):
  """Base class for WebDriver tests.

  Style note: snake_case is used for function names here so as to match
  with an internal class with the same name.
  """

  class LayoutTimeoutException(BaseException):
    """Exception thrown when layout did not complete in time."""

  def get_webdriver(self):
    return partial_layout_benchmark.GetWebDriver()

  def goto(self, path):
    """Goes to a path off of BASE_URL.

    Args:
      path: URL path
    Raises:
      Underlying WebDriver exceptions
    """
    self.get_webdriver().get(urlparse.urljoin(BASE_URL, path))

  def load_tv(self):
    """Loads the main TV page and waits for it to display.

    Raises:
      Underlying WebDriver exceptions
    """
    self.goto("")
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

  def wait_for_layout_complete(self):
    """Waits for Cobalt to complete pending layouts."""
    start_time = time.time()
    while int(self.get_webdriver().execute_script(
        "return h5vcc.cVal.getValue('Event.MainWebModule.IsProcessing')")):

      if time.time() - start_time > LAYOUT_TIMEOUT_SECONDS:
        raise TvTestCase.LayoutTimeoutException()

      time.sleep(0.1)

  def get_keyup_layout_duration_us(self):
    return int(self.get_webdriver().execute_script(
        "return h5vcc.cVal.getValue('Event.Duration.MainWebModule.KeyUp')"))

  def record_results(self, name, results):
    """Records results of benchmark.

    The duration of KeyUp events will be recorded.

    Args:
      name: name of test case
      results: Test results. Must be JSON encodable
    """
    print("tv_testcase RESULT: " + name + " "
          + json.JSONEncoder().encode(results))


def main():
  partial_layout_benchmark.main()
