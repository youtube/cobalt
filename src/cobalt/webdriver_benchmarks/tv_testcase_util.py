"""Provides constants and functions needed by tv_testcase classes."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import importlib
import json
import sys

# pylint: disable=C6204
from urllib import urlencode
import urlparse

import container_util

# These are watched for in webdriver_benchmark_test.py
TEST_RESULT = "webdriver_benchmark TEST RESULT"
TEST_COMPLETE = "webdriver_benchmark TEST COMPLETE"

# These are event types that can be injected
EVENT_TYPE_KEY_DOWN = "KeyDown"
EVENT_TYPE_KEY_UP = "KeyUp"

# URL-related constants
BASE_URL = "https://www.youtube.com/"
TV_APP_PATH = "/tv"
BASE_PARAMS = {}


def import_selenium_module(submodule=None):
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


def get_url(path, query_params=None):
  """Returns the URL indicated by the path and query parameters."""
  parsed_url = list(urlparse.urlparse(BASE_URL))
  parsed_url[2] = path
  query_dict = BASE_PARAMS.copy()
  if query_params:
    query_dict.update(urlparse.parse_qsl(parsed_url[4]))
    container_util.merge_dict(query_dict, query_params)
  parsed_url[4] = urlencode(query_dict, doseq=True)
  return urlparse.urlunparse(parsed_url)


def get_tv_url(query_params=None):
  """Returns the tv URL indicated by the query parameters."""
  return get_url(TV_APP_PATH, query_params)


def record_test_result(name, result):
  """Records an individual scalar result of a benchmark test.

  Args:
    name: string name of test case
    result: Test result. Must be JSON encodable scalar.
  """
  value_to_record = result

  string_value_to_record = json.JSONEncoder().encode(value_to_record)
  print("{}: {} {}".format(TEST_RESULT, name, string_value_to_record))
  sys.stdout.flush()


class RecordStrategyMean(object):
  """"Records the mean of an array of values."""

  def run(self, name, values):
    """Records the mean of an array of values.

    Args:
      name: string name of test case
      values: must be array of JSON encodable scalar
    """
    record_test_result("{}Mean".format(name), container_util.mean(values))


class RecordStrategyMedian(object):
  """"Records the median of an array of test results."""

  def run(self, name, values):
    """Records the median of an array of values.

    Args:
      name: string name of test case
      values: must be array of JSON encodable scalar
    """
    record_test_result("{}Median".format(name),
                       container_util.percentile(values, 50))


class RecordStrategyPercentile(object):
  """"Records the specified percentile of an array of test results."""

  def __init__(self, percentile):
    """Initializes the record strategy.

    Args:
      percentile: the percentile to record
    """
    self.percentile = percentile

  def run(self, name, values):
    """Records the percentile of an array of values.

    Args:
      name: string name of test case
      values: must be array of JSON encodable scalar
    Raises:
      RuntimeError: Raised on invalid args.
    """
    record_test_result("{}Pct{}".format(name, self.percentile),
                       container_util.percentile(values, self.percentile))


class RecordStrategyPercentiles(object):
  """"Records the 25th, 50th, 75th, and 95th percentiles of the values."""

  def run(self, name, values):
    """"Records the 25th, 50th, 75th, and 95th percentiles of the values.

    Args:
      name: string name of test case
      values: must be array of JSON encodable scalar
    Raises:
      RuntimeError: Raised on invalid args.
    """
    RecordStrategyPercentile(25).run(name, values)
    RecordStrategyPercentile(50).run(name, values)
    RecordStrategyPercentile(75).run(name, values)
    RecordStrategyPercentile(95).run(name, values)


class ResultsRecorder(object):
  """"Collects values and records results after a benchmark test ends."""

  def __init__(self, name, record_strategies):
    """Initializes the value recorder.

    Args:
      name: the name to use when recording test results
      record_strategies: the strategies to use when the test ends
    """
    self.name = name
    self.record_strategies = record_strategies
    self.values = []

  def collect_value(self, value):
    self.values.append(value)

  def on_end_test(self):
    """Handles logic related to the end of the benchmark test."""

    # Only run the strategies if values have been collected.
    if self.values:
      for record_strategy in self.record_strategies:
        record_strategy.run(self.name, self.values)
