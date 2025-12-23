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

#include "base/at_exit.h"
#include "base/command_line.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/test/test_launcher.h"
#include "starboard/event.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/test/test_timeouts.h"

namespace {

// This delegate is the bridge between the content::LaunchTests function and the
// Google Test framework.
class StarboardTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  // This method is called by content::LaunchTests when it's time to
  // execute the entire suite of discovered Google Tests.
  int RunTestSuite(int argc, char** argv) override {
    // RUN_ALL_TESTS() is the Google Test macro that discovers and runs
    // all tests linked into the executable.
    return RUN_ALL_TESTS();
  }

  // This is the required pure virtual function that we must implement.
  // It provides the main delegate for the content module, which is responsible
  // for setting up the content_shell environment for the tests.
  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new content::ContentBrowserTestShellMainDelegate();
  }
};

}  // namespace

// The C-style callback for the Starboard event loop. This must be in the
// global namespace to have the correct linkage for SbRunStarboardMain.
void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    // The Starboard platform is initialized and ready. It is now safe to
    // initialize and run the Chromium/gtest framework on this thread.
    SbEventStartData* start_data = static_cast<SbEventStartData*>(event->data);

    StarboardTestLauncherDelegate delegate;
    TestTimeouts::Initialize();
    
    // content::LaunchTests orchestrates the execution of browser tests.
    int test_result_code =
        content::LaunchTests(&delegate, 1, start_data->argument_count,
                             const_cast<char**>(start_data->argument_values));

    // After all tests have completed, request a graceful shutdown of the
    // Starboard application. This will cause SbRunStarboardMain to exit.
    SbSystemRequestStop(test_result_code);
  }
}

// The main entry point for the cobalt_browsertests executable.
int main(int argc, char** argv) {
  // Initialize base::CommandLine for any code that needs it early.
  base::CommandLine::Init(argc, argv);

  // Initialize the Google Test framework. This is crucial for discovering
  // tests and parsing gtest-specific flags (e.g., --gtest_filter).
  testing::InitGoogleTest(&argc, argv);

  // A manager for singleton destruction.
  base::AtExitManager at_exit;

  // This is a blocking call that hands control of the main thread over to the
  // Starboard event loop. The test logic will be kicked off from within
  // SbEventHandle when the kSbEventTypeStart event is received.
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
