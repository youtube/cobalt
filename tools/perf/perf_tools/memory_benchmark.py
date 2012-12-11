# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import multi_page_benchmark

MEMORY_HISTOGRAMS = [
    {'name': 'V8.MemoryExternalFragmentationTotal', 'units': 'percent'},
    {'name': 'V8.MemoryHeapSampleTotalCommitted', 'units': 'kb'},
    {'name': 'V8.MemoryHeapSampleTotalUsed', 'units': 'kb'}]

class MemoryBenchmark(multi_page_benchmark.MultiPageBenchmark):
  def __init__(self):
    super(MemoryBenchmark, self).__init__('stress_memory')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--dom-automation')

  def CanRunForPage(self, page):
    return hasattr(page, 'stress_memory')

  def MeasurePage(self, page, tab, results):
    for histogram in MEMORY_HISTOGRAMS:
      name = histogram['name']
      data = tab.runtime.Evaluate(
          'window.domAutomationController.getHistogram("%s")' % name)
      results.Add(name.replace('.', '_'), histogram['units'], data,
                  data_type='histogram')
