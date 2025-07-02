// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/blink_perf_test_suite.h"

#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

static int InitAndRunAllTests(int argc, char** argv) {
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
  return blink::BlinkPerfTestSuite(argc, argv).Run();
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
