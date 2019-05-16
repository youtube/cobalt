# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import smoothness_benchmark

class ScrollingBenchmark(smoothness_benchmark.SmoothnessBenchmark):
  def __init__(self):
    super(ScrollingBenchmark, self).__init__()
