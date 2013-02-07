# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import multi_page_benchmark
from telemetry import util

class JsGameBench(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, _, tab, results):
    tab.runtime.Execute('UI.call({}, "perftest")')

    js_is_done = 'document.getElementById("perfscore0") != null'
    def _IsDone():
      return bool(tab.runtime.Evaluate(js_is_done))
    util.WaitFor(_IsDone, 1200)

    js_get_results = 'document.getElementById("perfscore0").innerHTML'
    result = int(tab.runtime.Evaluate(js_get_results))
    results.Add('Score', 'score (bigger is better)', result)
