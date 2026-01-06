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

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
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

static ui::PlatformEventSourceStarboard* g_platform_event_source = nullptr;

// This delegate is the bridge between the content::LaunchTests function
// and the Google Test framework.
class StarboardTestLauncherDelegate : public content::TestLauncherDelegate {
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
void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    // The Starboard platform is initialized and ready. It is now safe
    // to initialize and run the Chromium/gtest framework on this
    // thread.
    SbEventStartData* start_data = static_cast<SbEventStartData*>(event->data);

    StarboardTestLauncherDelegate delegate;
    TestTimeouts::Initialize();

    base::InitStarboardTestMessageLoop();

    g_platform_event_source = new ui::PlatformEventSourceStarboard();

    int test_result_code =
        content::LaunchTests(&delegate, 1, start_data->argument_count,
                             const_cast<char**>(start_data->argument_values));

    // Manually stop the DevTools handlers. On Starboard, the
    // BrowserMainRunner is intentionally leaked and its Shutdown() method is
    // never called(see ShellMainDelegate::RunProcess), hence the handlers
    // are not stopped automatically. If we don't stop them here, they remain
    // alive at AtExit, causing a "Dangling Pointer" crash.
    content::ShellDevToolsManagerDelegate::StopHttpHandler();

    delete g_platform_event_source;
    g_platform_event_source = nullptr;

    SbSystemRequestStop(test_result_code);
  }
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);

  // A manager for singleton destruction.
  base::AtExitManager at_exit;

  // TODO(b/433354983): Support more platforms.
  ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());

  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
