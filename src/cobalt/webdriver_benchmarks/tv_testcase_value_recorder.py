#!/usr/bin/python2
""""Records stats on collected values during a benchmark test."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import tv_testcase_util


class ValueRecorder(object):
  """"Records stats on collected values during a benchmark test.

  Handles collecting values and records statistics on those values when the
  test ends.
  """

  def __init__(self, name):
    """Initializes the event benchmark recorder.

    Args:
      name: the name to use when recording test results
    """
    self.name = name
    self.values = []

  def collect_value(self, value):
    self.values.append(value)

  def on_end_test(self):
    """Handles logic related to the end of the benchmark test."""

    # Only record the test results when values have been collected.
    if self.values:
      self._record_test_results()

  def _record_test_result_mean(self):
    tv_testcase_util.record_test_result_mean("{}Mean".format(self.name),
                                             self.values)

  def _record_test_result_percentile(self, percentile):
    tv_testcase_util.record_test_result_percentile("{}Pct{}".format(
        self.name, percentile), self.values, percentile)

  def _record_test_results(self):
    """Records test results for the collected values.

    This subclass records the mean, 25th, 50th, 75th, and 95th percentiles of
    the collected values when the test ends.
    """
    self._record_test_result_mean()
    self._record_test_result_percentile(25)
    self._record_test_result_percentile(50)
    self._record_test_result_percentile(75)
    self._record_test_result_percentile(95)
