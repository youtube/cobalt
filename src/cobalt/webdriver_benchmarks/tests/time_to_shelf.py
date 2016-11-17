#!/usr/bin/python2
# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Calculate time to download YouTube TV app and initial layout of shelves."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import sys

# The parent directory is a module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

# pylint: disable=C6204,C6203
import tv_testcase


class TimeToShelf(tv_testcase.TvTestCase):

  def test_simple(self):
    """This test tries to measure the startup time for the YouTube TV page.

    Specifically, this test uses the Cobalt CVal Cobalt.Lifetime, which gets
    updated ~60Hz on a best effort basis.  This value is in microseconds.

    Note: t0 is defined after Cobalt starts up, but has not navigated to a page.
    If that true startup time metric is desired, perhaps a separate should be
    used.
    """
    metrics_array = []
    blank_startup_time_microseconds = self.get_int_cval('Cobalt.Lifetime')
    for _ in range(10):
      t0 = self.get_int_cval('Cobalt.Lifetime')
      self.load_tv()
      self.wait_for_layout_complete_after_focused_shelf()
      t1 = self.get_int_cval('Cobalt.Lifetime')
      startup_time_microseconds = t1 - t0
      metrics_array.append(startup_time_microseconds)

    self.record_results('TimeToShelf.test_time_shelf_display', metrics_array)
    self.record_results('TimeToShelf.blank_startup_time',
                        blank_startup_time_microseconds)


if __name__ == '__main__':
  tv_testcase.main()
