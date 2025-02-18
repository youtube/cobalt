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
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "cobalt/browser/switches.h"
#include "cobalt/cobalt_main_delegate.h"
#include "cobalt/platform_event_source_starboard.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/shell/browser/shell.h"
#include "starboard/event.h"

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
}  // namespace

int InitCobalt(int argc, const char** argv, const char* initial_deep_link) {
  const std::string initial_url =
      cobalt::switches::GetInitialURL(base::CommandLine(argc, argv));

  // content::ContentMainParams params(g_content_main_delegate.Get().get());
  content::ContentMainParams params(g_content_main_delegate);

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  const auto cobalt_args = std::to_array<const char*>(
      {// Disable first run experience, kiosk, etc.
       "--disable-fre", "--no-first-run", "--kiosk",
       // Enable Blink to work in overlay video mode
       "--force-video-overlays",
       // Disable multiprocess mode.
       "--single-process",
       // Disable Vulkan.
       "--disable-features=Vulkan",
       // Accelerated GL is blanket disabled for Linux. Ignore the GPU blocklist
       // to enable it.
       "--ignore-gpu-blocklist",
       // Force some ozone settings.
       "--ozone-platform=starboard", "--use-gl=angle", "--use-angle=gles-egl",
       // Set the default size for the content shell/starboard window.
       "--content-shell-host-window-size=1920x1080",
       // Enable remote Devtools access.
       "--remote-debugging-port=9222",
       "--remote-allow-origins=http://localhost:9222",
       // This flag is added specifically for m114 and should be removed after
       // rebasing to m120+
       "--user-level-memory-pressure-signal-params", "--no-sandbox",
       initial_url.c_str()});
  std::vector<const char*> args(argv, argv + argc);
  args.insert(args.end(), cobalt_args.begin(), cobalt_args.end());

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
