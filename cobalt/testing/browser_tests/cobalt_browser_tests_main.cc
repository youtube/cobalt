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

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

#include "base/allocator/partition_alloc_features.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/process/process.h"
#include "base/test/test_suite.h"
#include "base/test/test_support_starboard.h"
#include "base/test/test_timeouts.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#include "base/test/test_support_android.h"
#include "content/public/test/content_test_suite_base.h"
#endif
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
  int RunTestSuite(int argc, char** argv) override {
#if BUILDFLAG(IS_ANDROID)
    content::ContentTestSuiteBase::RegisterInProcessThreads();
#endif
    base::TestSuite test_suite(argc, argv);
    test_suite.DisableCheckForLeakedGlobals();
    return test_suite.Run();
  }

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

      LOG(ERROR) << "SbEventHandle argc: " << argc;
      for (int i = 0; i < argc; ++i) {
        LOG(ERROR) << "SbEventHandle argv[" << i << "]: " << argv[i];
      }

      base::CommandLine::Init(argc, argv);

#if BUILDFLAG(IS_POSIX)
      {
        base::CommandLine* command_line =
            base::CommandLine::ForCurrentProcess();
        if (command_line->HasSwitch("native-test-stdout")) {
          base::FilePath stdout_file_path =
              command_line->GetSwitchValuePath("native-test-stdout");
          if (freopen(stdout_file_path.value().c_str(), "a+", stdout) == NULL) {
            LOG(ERROR) << "Failed to redirect stdout to "
                       << stdout_file_path.value();
          } else {
            dup2(STDOUT_FILENO, STDERR_FILENO);
          }
        }
      }
#endif

      testing::InitGoogleTest(&argc, argv);

      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      std::string disabled_features =
          command_line->GetSwitchValueASCII("disable-features");
      if (disabled_features.empty()) {
        disabled_features = "PartitionAllocDanglingPtr";
      } else if (disabled_features.find("PartitionAllocDanglingPtr") ==
                 std::string::npos) {
        disabled_features += ",PartitionAllocDanglingPtr";
      }

      base::FieldTrialList field_trial_list;
      auto feature_list = std::make_unique<base::FeatureList>();
      feature_list->InitFromCommandLine(
          command_line->GetSwitchValueASCII("enable-features"),
          disabled_features);
      base::FeatureList::SetInstance(std::move(feature_list));

      // TODO(b/433354983): Support more platforms.
#if BUILDFLAG(IS_LINUX)
      ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());
#endif

      if (!s_delegate) {
        s_delegate = new CobaltBrowserTestLauncherDelegate();
      }
#if !BUILDFLAG(IS_ANDROID)
      base::InitStarboardTestMessageLoop();
#endif
#if BUILDFLAG(IS_ANDROID)
      base::i18n::AllowMultipleInitializeCallsForTesting();
      {
        base::FilePath external_dir;
        base::android::GetExternalStorageDirectory(&external_dir);
        base::FilePath test_data_dir =
            external_dir.Append("chromium_tests_root");
        base::InitAndroidTestPaths(test_data_dir);
      }
#endif
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

#if !SB_IS(EVERGREEN) && !BUILDFLAG(IS_ANDROID)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
