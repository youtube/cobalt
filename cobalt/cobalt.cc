// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <unistd.h>

#include <array>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "cobalt/browser/switches.h"
#include "cobalt/cobalt_main_delegate.h"
#include "cobalt/platform_event_source_starboard.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "sandbox/policy/switches.h"
#include "starboard/event.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

using starboard::PlatformEventSourceStarboard;

namespace {

content::ContentMainRunner* GetContentMainRunner() {
  static base::NoDestructor<std::unique_ptr<content::ContentMainRunner>> runner{
      content::ContentMainRunner::Create()};
  return runner->get();
}

static base::AtExitManager* g_exit_manager = nullptr;
static cobalt::CobaltMainDelegate* g_content_main_delegate = nullptr;
static PlatformEventSourceStarboard* g_platform_event_source = nullptr;

// Toggle-only Switches.
const auto kCobaltToggleSwitches = std::to_array<const char*>({
  // Disable first run experience, kiosk, etc.
  "disable-fre",
  switches::kNoFirstRun,
  switches::kKioskMode,
  // Enable Blink to work in overlay video mode
  switches::kForceVideoOverlays,
  // Disable multiprocess mode.
  switches::kSingleProcess,
  // Accelerated GL is blanket disabled for Linux. Ignore the GPU blocklist
  // to enable it.
  switches::kIgnoreGpuBlocklist,
  // This flag is added specifically for m114 and should be removed after
  // rebasing to m120+
#if BUILDFLAG(IS_ANDROID)
  switches::kUserLevelMemoryPressureSignalParams,
#endif  // BUILDFLAG(IS_ANDROID)
  sandbox::policy::switches::kNoSandbox
});

// Switches with parameters. May require special handling with passed in values.
const auto kCobaltParamSwitches = base::CommandLine::SwitchMap({
  // Disable Vulkan.
  {switches::kDisableFeatures, "Vulkan"},
  // Force some ozone settings.
  {switches::kOzonePlatform, "starboard"},
  {switches::kUseGL, "angle"},
  {switches::kUseANGLE, "gles-egl"},
  // Set the default size for the content shell/starboard window.
  {switches::kContentShellHostWindowSize, "1920x1080"},
  // Enable remote Devtools access.
  {switches::kRemoteDebuggingPort, "9222"},
  {switches::kRemoteAllowOrigins, "http://localhost:9222"},
});

class CommandLinePreprocessor {

 public:
  CommandLinePreprocessor(int argc, const char** argv) : cmd_line_(argc, argv) {
    // Insert these switches if they are missing.
    for (const auto& cobalt_switch : kCobaltToggleSwitches) {
      cmd_line_.AppendSwitch(cobalt_switch);
    }

    // Handle special-case switches with duplicated entries.
    if (cmd_line_.HasSwitch(switches::kDisableFeatures)) {
      // Merge all disabled features together.
      std::string disabled_features(cmd_line_.GetSwitchValueASCII(
          switches::kDisableFeatures));
      auto old_value = kCobaltParamSwitches.find(switches::kDisableFeatures);
      if (old_value != kCobaltParamSwitches.end()) {
        disabled_features += std::string(old_value->second);
        cmd_line_.AppendSwitchNative(switches::kDisableFeatures,
                                     disabled_features);
      }
    }
    if (cmd_line_.HasSwitch(switches::kContentShellHostWindowSize)) {
      // Replace with the passed in argument for window-size.
      if (cmd_line_.HasSwitch(switches::kWindowSize)) {
        cmd_line_.RemoveSwitch(switches::kContentShellHostWindowSize);
        cmd_line_.AppendSwitchASCII(switches::kContentShellHostWindowSize,
          cmd_line_.GetSwitchValueASCII(switches::kWindowSize));
      }
    }

    // Insert these switches if they are mising.
    for (const auto& iter : kCobaltParamSwitches) {
      const auto& switch_key = iter.first;
      const auto& switch_val = iter.second;
      if (!cmd_line_.HasSwitch(iter.first)) {
        cmd_line_.AppendSwitchNative(switch_key, switch_val);
      }
    }
  }

  const base::CommandLine::StringVector& argv() const {
    return cmd_line_.argv();
  }

  const std::string GetInitialURL() const {
    return cobalt::switches::GetInitialURL(cmd_line_);
  }

 private:
  base::CommandLine cmd_line_;

}; // CommandLinePreprocessor


}  // namespace

int InitCobalt(int argc, const char** argv, const char* initial_deep_link) {
  // content::ContentMainParams params(g_content_main_delegate.Get().get());
  content::ContentMainParams params(g_content_main_delegate);

  CommandLinePreprocessor init_cmd_line(argc, argv);
  const auto& init_argv = init_cmd_line.argv();
  std::vector<const char*> args;
  for (const auto& arg : init_argv) {
    args.push_back(arg.c_str());
  }

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  // This expression exists to ensure that we apply the argument overrides
  // only on the main process, not on spawned processes such as the zygote.
  if ((!strcmp(argv[0], "/proc/self/exe")) ||
      ((argc >= 2) && !strcmp(argv[1], "--type=zygote"))) {
    params.argc = argc;
    params.argv = argv;
  } else {
    params.argc = args.size();
    params.argv = args.data();
  }

  return RunContentProcess(std::move(params), GetContentMainRunner());
}

void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypePreload: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      g_exit_manager = new base::AtExitManager();
      g_content_main_delegate = new cobalt::CobaltMainDelegate();
      g_platform_event_source = new PlatformEventSourceStarboard();
      // TODO: (cobalt b/392613336) Initialize the musl hardware capabilities.
      // init_musl_hwcap();
      InitCobalt(data->argument_count,
                 const_cast<const char**>(data->argument_values), data->link);

      break;
    }
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      g_exit_manager = new base::AtExitManager();
      g_content_main_delegate = new cobalt::CobaltMainDelegate();
      g_platform_event_source = new PlatformEventSourceStarboard();
      // TODO: (cobalt b/392613336 Initialize the musl hardware capabilities.
      // init_musl_hwcap();
      InitCobalt(data->argument_count,
                 const_cast<const char**>(data->argument_values), data->link);
      break;
    }
    case kSbEventTypeStop: {
      content::Shell::Shutdown();

      g_content_main_delegate->Shutdown();

      GetContentMainRunner()->Shutdown();

      delete g_content_main_delegate;
      g_content_main_delegate = nullptr;

      delete g_platform_event_source;
      g_platform_event_source = nullptr;

      delete g_exit_manager;
      g_exit_manager = nullptr;
      break;
    }
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
    case kSbEventTypeConceal:
    case kSbEventTypeReveal:
    case kSbEventTypeFreeze:
    case kSbEventTypeUnfreeze:
      break;
    case kSbEventTypeInput:
      if (g_platform_event_source) {
        g_platform_event_source->HandleEvent(event);
      }
      break;
    case kSbEventTypeUser:
    case kSbEventTypeLink:
    case kSbEventTypeVerticalSync:
    case kSbEventTypeScheduled:
    case kSbEventTypeAccessibilitySettingsChanged:
    case kSbEventTypeLowMemory:
    case kSbEventTypeWindowSizeChanged:
    case kSbEventTypeOnScreenKeyboardShown:
    case kSbEventTypeOnScreenKeyboardHidden:
    case kSbEventTypeOnScreenKeyboardFocused:
    case kSbEventTypeOnScreenKeyboardBlurred:
    case kSbEventTypeAccessibilityCaptionSettingsChanged:
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged:
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
    case kSbEventDateTimeConfigurationChanged:
    case kSbEventTypeReserved1:
      break;
  }
}

int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
