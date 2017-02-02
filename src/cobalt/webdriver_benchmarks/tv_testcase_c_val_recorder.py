#!/usr/bin/python2
""""Records stats on the collected values of a CVal during a benchmark test."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import tv_testcase_util


class CValRecorderBase(object):
  """"Records stats on the collected values of a CVal during a benchmark test.

  Handles collecting values for a specific CVal during a benchmark test and
  records statistics on those values when the test ends. The base class does not
  record any statistics and subclasses determine which statistics to record.
  """

  def __init__(self, test, c_val_name, record_name):
    """Initializes the event benchmark recorder.

    Args:
      test: specific benchmark test being run
      c_val_name: the lookup name of the CVal
      record_name: the name to use when recording CVal stats for the test
    """
    self.test = test
    self.c_val_name = c_val_name
    self.record_name = record_name
    self.values = []

  def collect_current_value(self):
    self.values.append(self.test.get_int_cval(self.c_val_name))

  def on_end_test(self):
    """Handles logic related to the end of the benchmark test."""

    # Only record the test results when values have been collected.
    if self.values:
      self._record_test_results()

  def _record_test_results(self):
    """Records test results for the collected CVal values.

    This function is expected to be overridden by subclasses, which will use
    it to record select statistics for the test results.
    """
    pass

  def _record_test_result_mean(self):
    tv_testcase_util.record_test_result_mean("{}Mean".format(self.record_name),
                                             self.values)

  def _record_test_result_percentile(self, percentile):
    tv_testcase_util.record_test_result_percentile("{}Pct{}".format(
        self.record_name, percentile), self.values, percentile)


class FullCValRecorder(CValRecorderBase):
  """"Subclass that records full statistics of the collected values.

  This subclass records the mean, 25th, 50th, 75th, and 95th percentiles of the
  collected values when the test ends.
  """

  def _record_test_results(self):
    self._record_test_result_mean()
    self._record_test_result_percentile(25)
    self._record_test_result_percentile(50)
    self._record_test_result_percentile(75)
    self._record_test_result_percentile(95)


class MeanCValRecorder(CValRecorderBase):
  """"Subclass that records the mean of the collected values.

  This subclass only records the mean of the collected values when the test
  ends.
  """

  def _record_test_results(self):
    self._record_test_result_mean()


class PercentileCValRecorder(CValRecorderBase):
  """"Subclass that records a percentile of the collected values.

  This subclass records only records a single percentile of the collected values
  when the test ends.
  """

  def __init__(self, test, c_val_name, output_name, percentile_to_record):
    super(PercentileCValRecorder, self).__init__(test, c_val_name, output_name)
    self.percentile_to_record = percentile_to_record

  def _record_test_results(self):
    self._record_test_result_percentile(self.percentile_to_record)
