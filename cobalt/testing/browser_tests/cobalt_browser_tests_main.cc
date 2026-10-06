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
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/test/test_suite.h"
#include "base/test/test_support_starboard.h"
#include "base/test/test_timeouts.h"
#include "cobalt/app/cobalt_switch_defaults.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/test/test_launcher.h"
#include "starboard/event.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

#if SB_IS(EVERGREEN)
#include "ui/gl/gl_switches.h"
#endif

namespace {
// This delegate is the bridge between the content::LaunchTests function
// and the Google Test framework.
class CobaltBrowserTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  // This method is called by content::LaunchTests to
  // execute the entire suite of discovered Google Tests.
  int RunTestSuite(int argc, char** argv) override {
    // Intentionally leak TestSuite so ~TestSuite() does not destroy its
    // base::AtExitManager when RunTestSuite() returns. On Starboard in
    // single-process mode, ~RenderProcessHostImpl() releases
    // Chrome_InProcRendererThread without joining it and relies on std::_Exit()
    // in kSbEventTypeStop; running ~AtExitManager() before std::_Exit() races
    // with in-flight Mojo disconnect tasks on Chrome_InProcRendererThread.
    auto* test_suite = new base::TestSuite(argc, argv);
    test_suite->DisableCheckForLeakedGlobals();
    return test_suite->Run();
  }

  std::string GetUserDataDirectoryCommandLineSwitch() override {
    return switches::kContentShellUserDataDir;
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

#if SB_IS(EVERGREEN)
      // Production Cobalt runs CommandLinePreprocessor inside CobaltInit(), but
      // cobalt_browsertests enters through content::LaunchTests and
      // BrowserTestBase::SetUp(), which read base::CommandLine and
      // base::FeatureList before ContentMain runs. When launched directly via
      // loader_app on Evergreen, start_data only contains raw test runner
      // arguments without Cobalt's Starboard defaults (Ozone platform, ANGLE
      // GLES-EGL flags, and default feature overrides). Run
      // CommandLinePreprocessor here, strip the trailing positional startup URL
      // it appends so GTest/LaunchTests only sees program name and switches,
      // and transfer the resulting feature overrides into base::FeatureList.
      cobalt::CommandLinePreprocessor init_cmd_line(
          start_data->argument_count, start_data->argument_values);
      const auto& init_argv = init_cmd_line.argv();
      std::vector<char*> args;
      const size_t arg_count =
          init_argv.size() > 1 ? init_argv.size() - 1 : init_argv.size();
      args.reserve(arg_count);
      for (size_t i = 0; i < arg_count; ++i) {
        args.push_back(const_cast<char*>(init_argv[i].c_str()));
      }
      int argc = static_cast<int>(args.size());
      char** argv = args.data();
#else
      int argc = start_data->argument_count;
      char** argv = const_cast<char**>(start_data->argument_values);
#endif

      base::CommandLine::Init(argc, argv);
      testing::InitGoogleTest(&argc, argv);

      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      std::string disabled_features =
          command_line->GetSwitchValueASCII(switches::kDisableFeatures);
      if (disabled_features.empty()) {
        disabled_features = "PartitionAllocDanglingPtr";
      } else if (disabled_features.find("PartitionAllocDanglingPtr") ==
                 std::string::npos) {
        disabled_features += ",PartitionAllocDanglingPtr";
      }

#if SB_IS(EVERGREEN)
      // Strip ':param/value' field-trial parameter suffixes (such as
      // 'LimitImageDecodeCacheSize:mb/24' from CommandLinePreprocessor) before
      // initializing FeatureList. BrowserTestBase::SetUp() serializes
      // FeatureList overrides across a FieldTrialList reset into ContentMain,
      // which hits DCHECK(trial) for unactivated '<Study...' trial references.
      const std::string raw_enable_features =
          command_line->GetSwitchValueASCII(switches::kEnableFeatures);
      std::string enabled_features;
      for (const auto& entry :
           base::FeatureList::SplitFeatureListString(raw_enable_features)) {
        const auto colon_pos = entry.find(':');
        const std::string_view feature_name =
            colon_pos == std::string_view::npos ? entry
                                                : entry.substr(0, colon_pos);
        if (!enabled_features.empty()) {
          enabled_features += ',';
        }
        enabled_features.append(feature_name.data(), feature_name.size());
      }

      auto feature_list = std::make_unique<base::FeatureList>();
      feature_list->InitFromCommandLine(enabled_features, disabled_features);
      base::FeatureList::SetInstance(std::move(feature_list));

      // BrowserTestBase::SetUp() asserts that kEnableFeatures and
      // kDisableFeatures are not on the command line before copying active
      // features from base::FeatureList::GetInstance() back onto command_line.
      command_line->RemoveSwitch(switches::kEnableFeatures);
      command_line->RemoveSwitch(switches::kDisableFeatures);

      // Evergreen devices use native system EGL/GLES2 without software GL
      // (SwiftShader). Without kUseGpuInTests, BrowserTestBase::SetUp()
      // appends --override-use-software-gl-for-tests, causing GLContextEGL to
      // pass ANGLE-specific robustness attributes that fail with
      // EGL_BAD_ATTRIBUTE on native RDK EGL drivers.
      command_line->AppendSwitch(switches::kUseGpuInTests);
#else
      auto feature_list = std::make_unique<base::FeatureList>();
      feature_list->InitFromCommandLine(
          command_line->GetSwitchValueASCII(switches::kEnableFeatures),
          disabled_features);
      base::FeatureList::SetInstance(std::move(feature_list));
#endif

      // TODO(b/433354983): Support more platforms.
      ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());

      if (!s_delegate) {
        s_delegate = new CobaltBrowserTestLauncherDelegate();
      }
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
