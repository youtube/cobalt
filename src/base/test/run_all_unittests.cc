// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if defined(STARBOARD)
#include "starboard/client_porting/wrap_main/wrap_main.h"


int TestSuiteRun(int argc, char** argv) {
  base::AtExitManager exit_manager;
  return base::TestSuite(argc, argv).Run();
}

STARBOARD_WRAP_SIMPLE_MAIN(TestSuiteRun);

#else

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

#endif
