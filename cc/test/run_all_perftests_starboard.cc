// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/unit_test_launcher.h"
#include "cc/test/cc_test_suite.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "mojo/core/embedder/embedder.h"

static int InitAndRunAllTests(int argc, char** argv) {
  cc::CCTestSuite test_suite(argc, argv);
  mojo::core::Init();

  return base::LaunchUnitTestsSerially(argc, argv,
                               base::BindOnce(&cc::CCTestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif