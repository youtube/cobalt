// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

int TestSuiteRun(int argc, char** argv) {
  MainHook hook(NULL, argc, argv);
  return base::TestSuite(argc, argv).Run();
}

STARBOARD_WRAP_SIMPLE_MAIN(TestSuiteRun);
