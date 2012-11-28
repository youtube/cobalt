# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import multi_page_benchmark
from telemetry import util


class ImageDecoding(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, _, tab, results):
    def _IsDone():
      return tab.runtime.Evaluate('isDone')

    with tab.timeline.Recorder(tab.timeline):
      tab.runtime.Execute('runBenchmark()')
      util.WaitFor(_IsDone, 60)
    iterations = tab.runtime.Evaluate('minIterations')
    decode_image = tab.timeline.timeline_events.GetAllOfType('DecodeImage')
    elapsed_times = [d.elapsed_time for d in decode_image[-iterations:]]
    image_decoding_avg = sum(elapsed_times) / len(elapsed_times)
    results.Add('ImageDecoding_avg', 'ms', image_decoding_avg)
