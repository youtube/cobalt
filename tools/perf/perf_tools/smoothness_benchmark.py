# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import multi_page_benchmark
from telemetry import util

class DidNotScrollException(multi_page_benchmark.MeasurementFailure):
  def __init__(self):
    super(DidNotScrollException, self).__init__('Page did not scroll')

def GetOrZero(stat, rendering_stats_deltas):
  if stat in rendering_stats_deltas:
    return rendering_stats_deltas[stat]
  return 0

def DivideIfPossibleOrZero(numerator, denominator):
  if denominator == 0:
    return 0
  return numerator / denominator

def CalcScrollResults(rendering_stats_deltas, results):
  num_frames_sent_to_screen = rendering_stats_deltas['numFramesSentToScreen']

  mean_frame_time_seconds = (
    rendering_stats_deltas['totalTimeInSeconds'] /
      float(num_frames_sent_to_screen))

  dropped_percent = (
    rendering_stats_deltas['droppedFrameCount'] /
    float(num_frames_sent_to_screen))

  results.Add('mean_frame_time', 'ms', round(mean_frame_time_seconds * 1000, 3))
  results.Add('dropped_percent', '%', round(dropped_percent * 100, 1),
              data_type='unimportant')


def CalcPaintingResults(rendering_stats_deltas, results):
  totalPaintTime = GetOrZero('totalPaintTimeInSeconds',
                                   rendering_stats_deltas)

  totalRasterizeTime = GetOrZero('totalRasterizeTimeInSeconds',
                                       rendering_stats_deltas)

  totalPixelsPainted = GetOrZero('totalPixelsPainted',
                                       rendering_stats_deltas)

  totalPixelsRasterized = GetOrZero('totalPixelsRasterized',
                                          rendering_stats_deltas)

  megapixelsPaintedPerSecond = DivideIfPossibleOrZero(
      (totalPixelsPainted / 1000000.0), totalPaintTime)

  megapixelsRasterizedPerSecond = DivideIfPossibleOrZero(
      (totalPixelsRasterized / 1000000.0), totalRasterizeTime)

  results.Add('total_paint_time', 'seconds', totalPaintTime)
  results.Add('total_rasterize_time', 'seconds', totalRasterizeTime,
              data_type='unimportant')
  results.Add('total_pixels_painted', '', totalPixelsPainted,
              data_type='unimportant')
  results.Add('total_pixels_rasterized', '', totalPixelsRasterized,
              data_type='unimportant')
  results.Add('megapixels_painted_per_second', '', megapixelsPaintedPerSecond,
              data_type='unimportant')
  results.Add('megapixels_rasterized_per_second', '',
              megapixelsRasterizedPerSecond, data_type='unimportant')
  results.Add('total_paint_and_rasterize_time', 'seconds', totalPaintTime +
              totalRasterizeTime, data_type='unimportant')

def CalcTextureUploadResults(rendering_stats_deltas, results):
  if (('totalCommitCount' not in rendering_stats_deltas)
      or rendering_stats_deltas['totalCommitCount'] == 0) :
    averageCommitTimeMs = 0
  else :
    averageCommitTimeMs = (
        1000 * rendering_stats_deltas['totalCommitTimeInSeconds'] /
        rendering_stats_deltas['totalCommitCount'])

  results.Add('texture_upload_count', 'count',
              GetOrZero('textureUploadCount', rendering_stats_deltas))
  results.Add('average_commit_time', 'ms', averageCommitTimeMs,
              data_type='unimportant')

def CalcFirstPaintTimeResults(results, tab):
  if tab.browser.is_content_shell:
    results.Add('first_paint', 'ms', 'unsupported')
    return

  tab.runtime.Execute("""
      window.__rafFired = false;
      window.webkitRequestAnimationFrame(function() {
          window.__rafFired  = true;
      });
  """)
  util.WaitFor(lambda: tab.runtime.Evaluate('window.__rafFired'), 60)

  first_paint_secs = tab.runtime.Evaluate(
      'window.chrome.loadTimes().firstPaintTime - ' +
      'window.chrome.loadTimes().requestTime')

  results.Add('first_paint', 'ms', round(first_paint_secs * 1000, 1))

class SmoothnessBenchmark(multi_page_benchmark.MultiPageBenchmark):
  def __init__(self):
    super(SmoothnessBenchmark, self).__init__('scrolling')

  def AddCommandLineOptions(self, parser):
    parser.add_option('--no-gpu-benchmarking-extension', action='store_true',
        dest='no_gpu_benchmarking_extension',
        help='Disable the chrome.gpuBenchmarking extension.')
    parser.add_option('--report-all-results', dest='report_all_results',
                      action='store_true',
                      help='Reports all data collected, not just FPS')

  def CustomizeBrowserOptions(self, options):
    if not options.no_gpu_benchmarking_extension:
      options.extra_browser_args.append('--enable-gpu-benchmarking')

  def CanRunForPage(self, page):
    return hasattr(page, 'scrolling')

  def MeasurePage(self, page, tab, results):
    rendering_stats_deltas = tab.runtime.Evaluate(
      'window.__renderingStatsDeltas')

    if not (rendering_stats_deltas['numFramesSentToScreen'] > 0):
      raise DidNotScrollException()

    CalcFirstPaintTimeResults(results, tab)
    CalcScrollResults(rendering_stats_deltas, results)
    CalcPaintingResults(rendering_stats_deltas, results)
    CalcTextureUploadResults(rendering_stats_deltas, results)

    if self.options.report_all_results:
      for k, v in rendering_stats_deltas.iteritems():
        results.Add(k, '', v)
