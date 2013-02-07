#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry import multi_page_benchmark

_JS = 'chrome.gpuBenchmarking.printToSkPicture("{0}");'

class SkPicturePrinter(multi_page_benchmark.MultiPageBenchmark):
  def AddCommandLineOptions(self, parser):
    parser.add_option('-o', '--outdir', help='Output directory')

  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.extend(['--enable-gpu-benchmarking',
                                       '--no-sandbox'])

  def MeasurePage(self, page, tab, results):
    if self.options.outdir is not None:
      outpath = os.path.join(self.options.outdir, page.url_as_file_safe_name)
    outpath = os.path.abspath(outpath)
    # Replace win32 path separator char '\' with '\\'.
    js = _JS.format(outpath.replace('\\', '\\\\'))
    tab.runtime.Evaluate(js)
    results.Add('output_path', 'path', outpath)
