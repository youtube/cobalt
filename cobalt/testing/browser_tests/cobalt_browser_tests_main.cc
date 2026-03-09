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

#include <unistd.h>

#include <string>
#include <vector>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/test/test_support_starboard.h"
#include "base/test/test_timeouts.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/test/test_launcher.h"
#include "starboard/configuration.h"
#include "starboard/event.h"
#include "starboard/system.h"
// #include "starboard/shared/signal/suspend_signals.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

namespace {
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
SB_EXPORT void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    // The Starboard platform is initialized and ready. It is now safe
    // to initialize and run the Chromium/gtest framework on this
    // thread.
    SbEventStartData* start_data = static_cast<SbEventStartData*>(event->data);

    int argc = start_data->argument_count;
    char** argv = const_cast<char**>(start_data->argument_values);

    std::vector<char*> new_argv;
    for (int i = 0; i < argc; ++i) {
      new_argv.push_back(argv[i]);
    }
    char single_process_tests_arg[] = "--single-process-tests";
    char single_process_arg[] = "--single-process";
    char no_sandbox_arg[] = "--no-sandbox";
    char no_zygote_arg[] = "--no-zygote";
    char ozone_platform_arg[] = "--ozone-platform=starboard";
    new_argv.push_back(single_process_tests_arg);
    new_argv.push_back(single_process_arg);
    new_argv.push_back(no_sandbox_arg);
    new_argv.push_back(no_zygote_arg);
    new_argv.push_back(ozone_platform_arg);

    int new_argc = new_argv.size();
    char** new_argv_ptr = new_argv.data();

    base::CommandLine::Init(new_argc, new_argv_ptr);
    testing::InitGoogleTest(&new_argc, new_argv_ptr);

    // A manager for singleton destruction. We allocate this on the heap to
    // prevent it from going out of scope while background threads (like the
    // InProcRendererThread) are still tearing down during the final shutdown
    // sequence. Memory leaks on exit are acceptable here.
    new base::AtExitManager();

    // TODO(b/433354983): Support more platforms.
    ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());

    StarboardTestLauncherDelegate delegate;
    TestTimeouts::Initialize();

    base::InitStarboardTestMessageLoop();

    fprintf(stderr, ">>> lxn:::CALLING LaunchTests <<<\n");
    int test_result_code =
        content::LaunchTests(&delegate, 1, new_argc, new_argv_ptr);
    fprintf(stderr, ">>> lxn:::LaunchTests RETURNED %d <<<\n",
            test_result_code);

    // Disable PartitionAlloc dangling pointer checks on exit because the
    // browser test framework intentionally leaks state in single-process mode,
    // which triggers an ImmediateCrash() during AtExitManager teardown.
    // Disable all at-exit callbacks to avoid dangling pointer crashes.
    base::AtExitManager::DisableAllAtExitManagers();
    // Give Starboard a chance to shut down gracefully since we're inside the
    // event loop.
    // base::Process::TerminateCurrentProcessImmediately(test_result_code);
    // Not working. Signal 6

    SbSystemRequestStop(test_result_code);
    // starboard::UninstallSuspendSignalHandlers();
    // starboard::UninstallDebugSignalHandlers();
    // starboard::UninstallCrashSignalHandlers();
    // base::Process::TerminateCurrentProcessImmediately(test_result_code);
  }
}

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
