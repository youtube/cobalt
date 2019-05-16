# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json

from telemetry import multi_page_benchmark
from telemetry import util


class SunSpiderBenchark(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, _, tab, results):
    js_is_done = """
window.location.pathname.indexOf('sunspider-results') >= 0"""
    def _IsDone():
      return tab.runtime.Evaluate(js_is_done)
    util.WaitFor(_IsDone, 300, poll_interval=5)

    js_get_results = 'JSON.stringify(output);'
    js_results = json.loads(tab.runtime.Evaluate(js_get_results))
    r = collections.defaultdict(list)
    totals = []
    # js_results is: [{'foo': v1, 'bar': v2},
    #                 {'foo': v3, 'bar': v4},
    #                 ...]
    for result in js_results:
      total = 0
      for key, value in result.iteritems():
        r[key].append(value)
        total += value
      totals.append(total)
    for key, values in r.iteritems():
      results.Add('t', 'ms', values, chart_name=key, data_type='unimportant')
    results.Add('t', 'ms', totals, chart_name='total')
