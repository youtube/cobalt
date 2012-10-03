#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import tempfile
import shutil

from chrome_remote_control import multi_page_benchmark_unittest_base
from perf_tools import skpicture_printer

class SkPicturePrinterUnitTest(
  multi_page_benchmark_unittest_base.MultiPageBenchmarkUnitTestBase):

  def setUp(self):
    super(SkPicturePrinterUnitTest, self).setUp()
    self._outdir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._outdir)
    super(SkPicturePrinterUnitTest, self).tearDown()

  def CustomizeOptionsForTest(self, options):
    options.outdir = self._outdir

  def testPrintToSkPicture(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('non_scrollable_page.html')
    printer = skpicture_printer.SkPicturePrinter()
    all_results = self.RunBenchmark(printer, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))
    results0 = all_results.page_results[0]['results']

    outdir = results0['output_path']
    self.assertTrue('non_scrollable_page_html' in outdir)
    self.assertTrue(os.path.isdir(outdir))
    self.assertEqual(['layer_0.skp'], os.listdir(outdir))

    skp_file = os.path.join(outdir, 'layer_0.skp')
    self.assertTrue(os.path.isfile(skp_file))
    self.assertTrue(os.path.getsize(skp_file) > 0)
