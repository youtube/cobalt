"""Base class for WebDriver tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import logging
import math
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
import partial_layout_benchmark
import tv

# selenium imports
# pylint: disable=C0103
WebDriverWait = partial_layout_benchmark.ImportSeleniumModule(
    submodule="webdriver.support.ui").WebDriverWait

ElementNotVisibleException = (partial_layout_benchmark.ImportSeleniumModule(
    submodule="common.exceptions").ElementNotVisibleException)

BASE_URL = "https://www.youtube.com/"
TV_APP_PATH = "/tv"
BASE_PARAMS = {"env_forcedOffAllExperiments": True}
PAGE_LOAD_WAIT_SECONDS = 30
LAYOUT_TIMEOUT_SECONDS = 5


def _percentile(results, percentile):
  """Returns the percentile of an array.

  This method interpolates between two numbers if the percentile lands between
  two data points.

  Args:
      results: Sortable results array.
      percentile: A number ranging from 0-100.
  Returns:
      Appropriate value.
  Raises:
    RuntimeError: Raised on invalid args.
  """
  if not results:
    return None
  if percentile > 100 or percentile < 0:
    raise RuntimeError("percentile must be 0-100")
  sorted_results = sorted(results)

  if percentile == 100:
    return sorted_results[-1]
  fractional, index = math.modf((len(sorted_results) - 1) * (percentile * 0.01))
  index = int(index)

  if len(sorted_results) == index + 1:
    return sorted_results[index]

  return sorted_results[index] * (1 - fractional
                                 ) + sorted_results[index + 1] * fractional


def _merge_dict(merge_into, merge_from):
  """Merges the second dict into the first dict.

  Merge into differs from update in that it will not override values.  If the
  values already exist, the resulting value will be a list with a union of
  existing and new items.

  Args:
    merge_into: An output dict to merge values into.
    merge_from: An input dict to iterate over and insert values from.

  Returns:
    None
  """
  if not merge_from:
    return
  for k, v in merge_from.items():
    try:
      existing_value = merge_into[k]
    except KeyError:
      merge_into[k] = v
      continue

    if not isinstance(v, list):
      v = [v]
    if isinstance(existing_value, list):
      existing_value.extend(v)
    else:
      new_value = [existing_value]
      new_value.extend(v)
      merge_into[k] = new_value


class TvTestCase(unittest.TestCase):
  """Base class for WebDriver tests.

  Style note: snake_case is used for function names here so as to match
  with an internal class with the same name.
  """

  class LayoutTimeoutException(BaseException):
    """Exception thrown when layout did not complete in time."""

  @classmethod
  def setUpClass(cls):
    print("Running " + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    print("Done " + cls.__name__)

  def get_webdriver(self):
    return partial_layout_benchmark.GetWebDriver()

  def get_cval(self, cval_name):
    """Returns value of a cval.

    Args:
      cval_name: Name of the cval.
    Returns:
      Value of the cval.
    """
    javascript_code = "return h5vcc.cVal.getValue('{}')".format(cval_name)
    return self.get_webdriver().execute_script(javascript_code)

  def get_int_cval(self, cval_name):
    """Returns int value of a cval.

    The cval value must be an integer, if it is a float, then this function will
    throw a ValueError.

    Args:
      cval_name: Name of the cval.
    Returns:
      Value of the cval.
    Raises:
      ValueError if the cval is cannot be converted to int.
    """
    answer = self.get_cval(cval_name)
    if answer is None:
      return answer
    return int(answer)

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
      _merge_dict(query_dict, query_params)
    parsed_url[4] = urlencode(query_dict, doseq=True)
    final_url = urlparse.urlunparse(parsed_url)
    self.get_webdriver().get(final_url)

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

  def wait_for_layout_complete(self):
    """Waits for Cobalt to complete pending layouts."""
    start_time = time.time()
    while self.get_int_cval("Event.MainWebModule.IsProcessing"):

      if time.time() - start_time > LAYOUT_TIMEOUT_SECONDS:
        raise TvTestCase.LayoutTimeoutException()

      time.sleep(0.1)

  def get_keyup_layout_duration_us(self):
    return self.get_int_cval("Event.Duration.MainWebModule.KeyUp")

  def wait_for_layout_complete_after_focused_shelf(self):
    """Waits for Cobalt to focus on a shelf and complete pending layouts."""
    self.poll_until_found(tv.FOCUSED_SHELF)
    self.assert_displayed(tv.FOCUSED_SHELF_TITLE)
    self.wait_for_layout_complete()

  def record_result(self, name, result):
    """Records an individual scalar result of a benchmark.

    Args:
      name: name of test case
      result: Test result. Must be JSON encodable scalar.
    """
    value_to_record = result

    string_value_to_record = json.JSONEncoder().encode(value_to_record)
    print("tv_testcase RESULT: {} {}".format(name, string_value_to_record))

  def record_result_median(self, name, results):
    """Records the median of an array of results.

    Args:
      name: name of test case
      results: Test results array. Must be array of JSON encodable scalar.
    """
    value_to_record = _percentile(results, 50)

    string_value_to_record = json.JSONEncoder().encode(value_to_record)
    print("tv_testcase RESULT: {} {}".format(name, string_value_to_record))

  def record_result_percentile(self, name, results, percentile):
    """Records the percentile of an array of results.

    Args:
      name: The (string) name of test case.
      results: Test results array. Must be array of JSON encodable scalars.
      percentile: A number ranging from 0-100.
    Raises:
      RuntimeError: Raised on invalid args.
    """
    value_to_record = _percentile(results, percentile)
    string_value_to_record = json.JSONEncoder().encode(value_to_record)
    print("tv_testcase RESULT: {} {}".format(name, string_value_to_record))


def main():
  logging.basicConfig(level=logging.DEBUG)
  partial_layout_benchmark.main()
