"""Provides constants and functions needed by tv_testcase classes."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import importlib
import json
import sys

# pylint: disable=C6204
import urlparse
from urllib import urlencode

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
    name: name of test case
    result: Test result. Must be JSON encodable scalar.
  """
  value_to_record = result

  string_value_to_record = json.JSONEncoder().encode(value_to_record)
  print("{}: {} {}".format(TEST_RESULT, name, string_value_to_record))
  sys.stdout.flush()


def record_test_result_mean(name, results):
  """Records the mean of an array of results.

  Args:
    name: name of test case
    results: Test results array. Must be array of JSON encodable scalar.
  """
  record_test_result(name, container_util.mean(results))


def record_test_result_median(name, results):
  """Records the median of an array of results.

  Args:
    name: name of test case
    results: Test results array. Must be array of JSON encodable scalar.
  """
  record_test_result(name, container_util.percentile(results, 50))


def record_test_result_percentile(name, results, percentile):
  """Records the percentile of an array of results.

  Args:
    name: The (string) name of test case.
    results: Test results array. Must be array of JSON encodable scalars.
    percentile: A number ranging from 0-100.
  Raises:
    RuntimeError: Raised on invalid args.
  """
  record_test_result(name, container_util.percentile(results, percentile))
