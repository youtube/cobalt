# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging


# Language values match constants in Sponge protocol buffer (sponge.proto).
JAVA = 5
PYTHON = 7


class BaseTestResult(object):
  """A single result from a unit test."""

  def __init__(self, name, log):
    self.name = name
    self.log = log


class SingleTestResult(BaseTestResult):
  """Result information for a single test.

  Args:
    full_name: Full name of the test.
    start_date: Date in milliseconds when the test began running.
    dur: Duration of the test run in milliseconds.
    lang: Language of the test (JAVA or PYTHON).
    log: An optional string listing any errors.
    error: A tuple of a short error message and a longer version used by Sponge
    if test resulted in a fail or error.  An empty tuple implies a pass.
  """

  def __init__(self, full_name, start_date, dur, lang, log='', error=()):
    BaseTestResult.__init__(self, full_name, log)
    name_pieces = full_name.rsplit('#')
    if len(name_pieces) > 0:
      self.test_name = name_pieces[1]
      self.class_name = name_pieces[0]
    else:
      self.class_name = full_name
      self.test_name = full_name
    self.start_date = start_date
    self.dur = dur
    self.error = error
    self.lang = lang


class TestResults(object):
  """Results of a test run."""

  def __init__(self):
    self.ok = []
    self.failed = []
    self.crashed = []
    self.unknown = []
    self.disabled = []
    self.unexpected_pass = []
    self.timed_out = False
    self.overall_fail = False

  @staticmethod
  def FromRun(ok=None, failed=None, crashed=None, timed_out=False,
              overall_fail=False):
    ret = TestResults()
    ret.ok = ok or []
    ret.failed = failed or []
    ret.crashed = crashed or []
    ret.timed_out = timed_out
    ret.overall_fail = overall_fail
    return ret

  @staticmethod
  def FromTestResults(results):
    """Combines a list of results in a single TestResults object."""
    ret = TestResults()
    for t in results:
      ret.ok += t.ok
      ret.failed += t.failed
      ret.crashed += t.crashed
      ret.unknown += t.unknown
      ret.disabled += t.disabled
      ret.unexpected_pass += t.unexpected_pass
      if t.timed_out:
        ret.timed_out = True
      if t.overall_fail:
        ret.overall_fail = True
    return ret

  def _Log(self, sorted_list):
    for t in sorted_list:
      logging.critical(t.name)
      if t.log:
        logging.critical(t.log)

  def GetAllBroken(self):
    """Returns the all broken tests including failed, crashed, unknown."""
    return self.failed + self.crashed + self.unknown

  def LogFull(self):
    """Output all broken tests or 'passed' if none broken"""
    logging.critical('*' * 80)
    logging.critical('Final result')
    if self.failed:
      logging.critical('Failed:')
      self._Log(sorted(self.failed))
    if self.crashed:
      logging.critical('Crashed:')
      self._Log(sorted(self.crashed))
    if self.unknown:
      logging.critical('Unknown:')
      self._Log(sorted(self.unknown))
    if not self.GetAllBroken():
      logging.critical('Passed')
    logging.critical('*' * 80)

    # Summarize in the test output.
    summary_string = 'Summary:\n'
    summary_string += 'RAN=%d\n' % (len(self.ok) + len(self.failed) +
                                    len(self.crashed) + len(self.unknown))
    summary_string += 'PASSED=%d\n' % (len(self.ok))
    summary_string += 'FAILED=%d %s\n' % (len(self.failed),
                                          [t.name for t in self.failed])
    summary_string += 'CRASHED=%d %s\n' % (len(self.crashed),
                                           [t.name for t in self.crashed])
    summary_string += 'UNKNOWN=%d %s\n' % (len(self.unknown),
                                           [t.name for t in self.unknown])
    logging.critical(summary_string)
    return summary_string
