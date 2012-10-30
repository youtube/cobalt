# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import util

def _Mean(l):
  return float(sum(l)) / len(l) if len(l) > 0 else 0.0

class Kraken(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, _, tab, results):
    js_is_done = """
document.title.indexOf("Results") != -1 && document.readyState == "complete"
"""
    def _IsDone():
      return bool(tab.runtime.Evaluate(js_is_done))
    util.WaitFor(_IsDone, 300)

    js_get_results = """
var formElement = document.getElementsByTagName("input")[0];
decodeURIComponent(formElement.value.split("?")[1]);
"""
    result_dict = eval(tab.runtime.Evaluate(js_get_results))
    total = 0
    for key in result_dict:
      if key == 'v':
        continue
      results.Add(key, 'ms', result_dict[key])
      total += _Mean(result_dict[key])
    results.Add('Total', 'ms', total)
