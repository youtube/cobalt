# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import multi_page_benchmark
from telemetry import multi_page_benchmark_unittest_base
from telemetry import page
from perf_tools import smoothness_benchmark

from telemetry import browser_finder
from telemetry import options_for_unittests

import os
import urlparse

class SmoothnessBenchmarkUnitTest(
  multi_page_benchmark_unittest_base.MultiPageBenchmarkUnitTestBase):

  def testFirstPaintTimeMeasurement(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = smoothness_benchmark.SmoothnessBenchmark()
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))

    results0 = all_results.page_results[0]
    if results0['first_paint'] == 'unsupported':
      # This test can't run on content_shell.
      return
    self.assertTrue(results0['first_paint'] > 0)

  def testScrollingWithGpuBenchmarkingExtension(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = smoothness_benchmark.SmoothnessBenchmark()
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))
    results0 = all_results.page_results[0]

    self.assertTrue('dropped_percent' in results0)
    self.assertTrue('mean_frame_time' in results0)

  def testCalcResultsFromRAFRenderStats(self):
    rendering_stats = {'droppedFrameCount': 5,
                       'totalTimeInSeconds': 1,
                       'numAnimationFrames': 10,
                       'numFramesSentToScreen': 10}
    res = multi_page_benchmark.BenchmarkResults()
    res.WillMeasurePage(page.Page('http://foo.com/'))
    smoothness_benchmark.CalcScrollResults(rendering_stats, res)
    res.DidMeasurePage()
    self.assertEquals(50, res.page_results[0]['dropped_percent'])
    self.assertAlmostEquals(100, res.page_results[0]['mean_frame_time'], 2)

  def testCalcResultsRealRenderStats(self):
    rendering_stats = {'numFramesSentToScreen': 60,
                       'globalTotalTextureUploadTimeInSeconds': 0,
                       'totalProcessingCommandsTimeInSeconds': 0,
                       'globalTextureUploadCount': 0,
                       'droppedFrameCount': 0,
                       'textureUploadCount': 0,
                       'numAnimationFrames': 10,
                       'totalPaintTimeInSeconds': 0.35374299999999986,
                       'globalTotalProcessingCommandsTimeInSeconds': 0,
                       'totalTextureUploadTimeInSeconds': 0,
                       'totalRasterizeTimeInSeconds': 0,
                       'totalTimeInSeconds': 1.0}
    res = multi_page_benchmark.BenchmarkResults()
    res.WillMeasurePage(page.Page('http://foo.com/'))
    smoothness_benchmark.CalcScrollResults(rendering_stats, res)
    res.DidMeasurePage()
    self.assertEquals(0, res.page_results[0]['dropped_percent'])
    self.assertAlmostEquals(1000/60., res.page_results[0]['mean_frame_time'], 2)

  def testBoundingClientRect(self):
    options = options_for_unittests.GetCopy()
    browser_to_create = browser_finder.FindBrowser(options)
    if not browser_to_create:
      raise Exception('No browser found, cannot continue test.')

    with browser_to_create.Create() as browser:
      with browser.ConnectToNthTab(0) as tab:
        ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
        parsed_url = urlparse.urlparse(ps.pages[0].url)
        path = os.path.join(parsed_url.netloc, parsed_url.path)
        dirname, filename = os.path.split(path)
        dirname = os.path.join(ps.base_dir, dirname[1:])
        browser.SetHTTPServerDirectory(dirname)
        target_side_url = browser.http_server.UrlOf(filename)
        tab.page.Navigate(target_side_url)

        # Verify that the rect returned by getBoundingVisibleRect() in
        # scroll.js is completely contained within the viewport. Scroll
        # events dispatched by the benchmarks use the center of this rect
        # as their location, and this location needs to be within the
        # viewport bounds to correctly decide between main-thread and
        # impl-thread scrolling. If the scrollable area were not clipped
        # to the viewport bounds, then the instance used here (the scrollable
        # area being more than twice as tall as the viewport) would
        # result in a scroll location outside of the viewport bounds.
        tab.runtime.Execute("""document.body.style.height =
                               (2 * window.innerHeight + 1) + 'px';""")
        scroll_js_path = os.path.join(os.path.dirname(__file__), '..', '..',
                                      'telemetry', 'telemetry', 'scroll.js')
        scroll_js = open(scroll_js_path, 'r').read()
        tab.runtime.Evaluate(scroll_js)

        rect_bottom = int(tab.runtime.Evaluate("""
                  __ScrollTest_GetBoundingVisibleRect(document.body).top +
                  __ScrollTest_GetBoundingVisibleRect(document.body).height"""))
        rect_right = int(tab.runtime.Evaluate("""
                  __ScrollTest_GetBoundingVisibleRect(document.body).left +
                  __ScrollTest_GetBoundingVisibleRect(document.body).width"""))
        viewport_width = int(tab.runtime.Evaluate('window.innerWidth'))
        viewport_height = int(tab.runtime.Evaluate('window.innerHeight'))

        self.assertTrue(rect_bottom <= viewport_height)
        self.assertTrue(rect_right <= viewport_width)

  def testDoesImplThreadScroll(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = smoothness_benchmark.SmoothnessBenchmark()
    benchmark.force_enable_threaded_compositing = True
    all_results = self.RunBenchmark(benchmark, ps)

    results0 = all_results.page_results[0]
    self.assertTrue(results0['percent_impl_scrolled'] > 0)

  def testScrollingWithoutGpuBenchmarkingExtension(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = smoothness_benchmark.SmoothnessBenchmark()
    benchmark.use_gpu_benchmarking_extension = False
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))
    results0 = all_results.page_results[0]

    self.assertTrue('dropped_percent' in results0)
    self.assertTrue('mean_frame_time' in results0)
