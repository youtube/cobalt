# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import multi_page_benchmark
from telemetry import util

class SpaceportBenchmark(multi_page_benchmark.MultiPageBenchmark):
  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.extend(['--disable-gpu-vsync'])

  def MeasurePage(self, _, tab, results):
    tab.runtime.Execute("""
        window.__results = {};
        window.console.log = function(str) {
            if (!str) return;
            var key_val = str.split(': ');
            if (!key_val.length == 2) return;
            __results[key_val[0]] = key_val[1];
        };
        document.getElementById('start-performance-tests').click();
    """)

    js_get_results = 'JSON.stringify(window.__results)'
    def _IsDone():
      num_tests_in_benchmark = 24
      result_dict = eval(tab.runtime.Evaluate(js_get_results))
      return num_tests_in_benchmark == len(result_dict)
    util.WaitFor(_IsDone, 1200)

    result_dict = eval(tab.runtime.Evaluate(js_get_results))
    for key in result_dict:
      chart, trace = key.split('.', 1)
      results.Add(trace, 'objects (bigger is better)', float(result_dict[key]),
                  chart_name=chart, data_type='unimportant')
    results.Add('Overall', 'objects (bigger is better)',
                [float(x) for x in result_dict.values()])
