// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "base/metrics/histogram.h"
#include "base/nss_util.h"
#include "net/base/net_test_suite.h"

int main(int argc, char** argv) {
  // Record histograms, so we can get histograms data in tests.
  base::StatisticsRecorder recorder;
  NetTestSuite test_suite(argc, argv);

#if defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  base::EnsureNSPRInit();
#endif

  return test_suite.Run();
}
