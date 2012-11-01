# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

class ThermalThrottle(object):
  """Class to detect and track thermal throttling

  Usage:
    Wait for IsThrottled() to be False before running test
    After running test call HasBeenThrottled() to find out if the
    test run was affected by thermal throttling.

    Currently assumes an OMap device.
  """
  def __init__(self, adb):
    self._adb = adb
    self._throttled = False


  def HasBeenThrottled(self):
    """ True if there has been any throttling since the last call to
        HasBeenThrottled or IsThrottled
    """
    return self._ReadLog()

  def IsThrottled(self):
    """True if currently throttled"""
    self._ReadLog()
    return self._throttled

  def _ReadLog(self):
    has_been_throttled = False
    log = self._adb.RunShellCommand('dmesg -c')
    for line in log:
      if 'omap_thermal_throttle' in line:
        if not self._throttled:
          logging.warning('>>> Thermally Throttled')
        self._throttled = True
        has_been_throttled = True
      if 'omap_thermal_unthrottle' in line:
        if self._throttled:
          logging.warning('>>> Thermally Unthrottled')
        self._throttled = False
        has_been_throttled = True
    return has_been_throttled
