# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import util

class RobohornetPro(multi_page_benchmark.MultiPageBenchmark):
  def CustomizeBrowserOptions(self, options):
    # Benchmark require use of real Date.now() for measurement.
    options.wpr_make_javascript_deterministic = False

  def MeasurePage(self, _, tab, results):
    tab.runtime.Execute('ToggleRoboHornet()')

    done = 'document.getElementById("results").innerHTML.indexOf("Total") != -1'
    def _IsDone():
      return tab.runtime.Evaluate(done)
    util.WaitFor(_IsDone, 60)

    result = int(tab.runtime.Evaluate('stopTime - startTime'))
    results.Add('Total', 'ms', result)
