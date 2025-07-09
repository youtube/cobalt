// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <signal.h>

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "mojo/core/test/mojo_test_suite_base.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "testing/gtest/include/gtest/gtest.h"

static int InitAndRunAllTests(int argc, char** argv) {
#if !BUILDFLAG(IS_ANDROID)
  // Silence death test thread warnings on Linux. We can afford to run our death
  // tests a little more slowly (< 10 ms per death test on a Z620).
  // On android, we need to run in the default mode, as the threadsafe mode
  // relies on execve which is not available.
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
#endif
#if BUILDFLAG(IS_ANDROID)
  // On android, the test framework has a signal handler that will print a
  // [ CRASH ] line when the application crashes. This breaks death test has the
  // test runner will consider the death of the child process a test failure.
  // Removing the signal handler solves this issue.
  signal(SIGABRT, SIG_DFL);
#endif

  mojo::core::test::MojoTestSuiteBase test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
