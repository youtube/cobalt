// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include "base/test/allow_check_is_test_for_testing.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

static int InitAndRunAllTests(int argc, char** argv) {
  base::test::AllowCheckIsTestForTesting();
  mojo::core::Init();
  return base::TestSuite(argc, argv).Run();
}

// For the Starboard OS define SbEventHandle as the entry point
SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
// Define main() for non-Evergreen Starboard OS.
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif  // !SB_IS(EVERGREEN)
#else
int main(int argc, char** argv) {
  int result = 0;
  {
    base::TestSuite test_suite(argc, argv);
    mojo::core::Init();
    result = base::LaunchUnitTests(
        argc, argv,
        base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
  }
  return result;
}
#endif  // BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
