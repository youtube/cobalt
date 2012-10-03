#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import re

from chrome_remote_control import multi_page_benchmark

_JS = 'chrome.gpuBenchmarking.printToSkPicture("{0}");'

class SkPicturePrinter(multi_page_benchmark.MultiPageBenchmark):
  def AddOptions(self, parser):
    parser.add_option('-o', '--outdir', help='Output directory')

  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.extend(['--enable-gpu-benchmarking',
                                       '--no-sandbox'])

  def MeasurePage(self, page, tab):
    # Derive output path from the URL. The pattern just replaces all special
    # characters in the url with underscore.
    outpath = re.sub('://|[.~!*\:@&=+$,/?%#]', '_', page.url)
    if self.options.outdir is not None:
      outpath = os.path.join(self.options.outdir, outpath)
    outpath = os.path.abspath(outpath)
    # Replace win32 path separator char '\' with '\\'.
    js = _JS.format(outpath.replace('\\', '\\\\'))
    tab.runtime.Evaluate(js)
    return {'output_path': outpath}

def Main():
  return multi_page_benchmark.Main(SkPicturePrinter())
