// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/perf_test_suite.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

static int InitAndRunAllTests(int argc, char** argv) {
  return base::PerfTestSuite(argc, argv).Run();
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
