// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.

#include <cstdlib>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/test/test_support_starboard.h"
#include "base/test/test_timeouts.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/test/test_launcher.h"
#include "starboard/event.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

namespace {
// This delegate is the bridge between the content::LaunchTests function
// and the Google Test framework.
class CobaltBrowserTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  // This method is called by content::LaunchTests to
  // execute the entire suite of discovered Google Tests.
  int RunTestSuite(int argc, char** argv) override { return RUN_ALL_TESTS(); }

  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new content::ContentBrowserTestShellMainDelegate();
  }
};

}  // namespace

// The C-style callback for the Starboard event loop. This must be in
// the global namespace to have the correct linkage for
// SbRunStarboardMain.

SB_EXPORT void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      // The Starboard platform is initialized and ready. It is now safe
      // to initialize and run the Chromium/gtest framework on this
      // thread.
      SbEventStartData* start_data =
          static_cast<SbEventStartData*>(event->data);

      int argc = start_data->argument_count;
      char** argv = const_cast<char**>(start_data->argument_values);

      base::CommandLine::Init(argc, argv);
      testing::InitGoogleTest(&argc, argv);

      // A manager for singleton destruction.
      base::AtExitManager at_exit;

      // TODO(b/433354983): Support more platforms.
      ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());

      CobaltBrowserTestLauncherDelegate delegate;
      TestTimeouts::Initialize();
      base::InitStarboardTestMessageLoop();
      int test_result_code = content::LaunchTests(&delegate, 1, argc, argv);

      // Call std::_Exit() from <cstdlib> to immediately terminate the process
      // without executing any C++ destructors or AtExitManager callbacks.
      // Chromium browser tests intentionally leak state in single-process mode,
      // which causes memory access violations during standard teardown by
      // Starboard.
      //
      // Note: We cannot use standard _exit() (lowercase) or Chromium's
      // base::Process::TerminateCurrentProcessImmediately (which calls _exit)
      // because Starboard's Musl port specifically maps _exit() back to exit(),
      // which runs the problematic teardown logic anyway. std::_Exit()
      // (uppercase) bypasses this and invokes the raw SYS_exit_group syscall.
      // TODO(b/463991461): Consider ASAN_OPTIONS=exitcode=0 to avoid the need
      // for std::_Exit() here.
      std::_Exit(test_result_code);
    }
    default:
      break;
  }
}

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
