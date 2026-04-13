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
  static int s_test_result_code = 0;
  static base::AtExitManager* s_at_exit_manager = nullptr;
  static CobaltBrowserTestLauncherDelegate* s_delegate = nullptr;

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
      if (!s_at_exit_manager) {
        s_at_exit_manager = new base::AtExitManager();
      }

      // TODO(b/433354983): Support more platforms.
      ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());

      if (!s_delegate) {
        s_delegate = new CobaltBrowserTestLauncherDelegate();
      }
      TestTimeouts::Initialize();
      base::InitStarboardTestMessageLoop();
      s_test_result_code = content::LaunchTests(s_delegate, 1, argc, argv);
      SbSystemRequestStop(s_test_result_code);
      break;
    }
    case kSbEventTypeStop: {
      // We must use std::_Exit() from <cstdlib> to immediately terminate the
      // process without executing any C++ destructors or Starboard's teardown
      // callbacks.
      //
      // 1. Returning naturally: Chromium browser tests intentionally leak state
      //    in single-process mode. Starboard's teardown sequence triggers
      //    Chromium's Dangling Pointer Detector and causes a SIGABRT/SIGSEGV.
      //    ASAN_OPTIONS cannot suppress this because Starboard uninstalls
      //    ASAN's signal handlers during teardown.
      // 2. TerminateCurrentProcessImmediately(): Chromium's base::Process
      //    implementation calls the standard library's `_exit()`. However,
      //    the Evergreen ELF loader sandbox explicitly does not export `_Exit`
      //    or `_exit`, causing the loader to abort when it attempts to resolve
      //    the symbol.
      //
      // std::_Exit() (uppercase) bypasses the C library entirely and invokes
      // the raw SYS_exit_group syscall, escaping the sandbox and terminating
      // cleanly.
      std::_Exit(s_test_result_code);
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
