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
#include <sstream>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "cobalt/app/cobalt_switch_defaults_starboard.h"
#include "cobalt/browser/cobalt_content_browser_client.h"
#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "starboard/event.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

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
  // content::ContentMainParams params(g_content_main_delegate.Get().get());
  content::ContentMainParams params(g_content_main_delegate);

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  // Preprocess the raw command line arguments with the defaults expected by
  // Cobalt.
  cobalt::CommandLinePreprocessor init_cmd_line(argc, argv);
  const auto& init_argv = init_cmd_line.argv();

  std::stringstream ss;
  std::vector<const char*> args;
  for (const auto& arg : init_argv) {
    args.push_back(arg.c_str());
    ss << " " << arg;
  }
  LOG(INFO) << "Parsed command line string:" << ss.str();

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

  base::FilePath content_shell_data_path;
  base::PathService::Get(base::DIR_CACHE, &content_shell_data_path);
  constexpr char cobalt_subdir[] = "cobalt";
  if (content_shell_data_path.BaseName().value() != cobalt_subdir) {
    content_shell_data_path = content_shell_data_path.Append(cobalt_subdir);
    base::PathService::OverrideAndCreateIfNeeded(
        base::DIR_CACHE, content_shell_data_path,
        /*is_absolute=*/true, /*create=*/true);
  }
  base::PathService::OverrideAndCreateIfNeeded(
      content::SHELL_DIR_USER_DATA, content_shell_data_path,
      /*is_absolute=*/true, /*create=*/true);

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
    case kSbEventTypeBlur: {
      auto* client = cobalt::CobaltContentBrowserClient::Get();
      if (client) {
        client->DispatchBlur();
      }
      CHECK(g_platform_event_source);
      g_platform_event_source->HandleEvent(event);
      break;
    }
    case kSbEventTypeFocus: {
      auto* client = cobalt::CobaltContentBrowserClient::Get();
      if (client) {
        client->DispatchFocus();
      }
      CHECK(g_platform_event_source);
      g_platform_event_source->HandleEvent(event);
      break;
    }
    case kSbEventTypeConceal:
    case kSbEventTypeReveal:
      break;
    case kSbEventTypeFreeze: {
      auto* client = cobalt::CobaltContentBrowserClient::Get();
      if (client) {
        client->FlushCookiesAndLocalStorage();
      }
      break;
    }
    case kSbEventTypeUnfreeze:
      break;
    case kSbEventTypeInput: {
      if (g_platform_event_source) {
        g_platform_event_source->HandleEvent(event);
      }
      break;
    }
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged: {
      if (event->data) {
        auto* enabled = static_cast<const bool*>(event->data);
        cobalt::browser::H5vccAccessibilityManager::GetInstance()
            ->OnTextToSpeechStateChanged(*enabled);
      }
      break;
    }
<<<<<<< HEAD
    case kSbEventTypeLink:
    case kSbEventTypeVerticalSync:
=======
    case kSbEventTypeLowMemory: {
      // Send a one-time critical memory pressure signal to ask
      // other components to release memory.
      base::MemoryPressureListener::NotifyMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
      LOG(INFO) << "Firing a criticial memory pressure signal to reduce memory "
                   "burden.";

      // Chromium internally calls Reclaim/ReclaimNormal at regular interval
      // to claim free memory. Using ReclaimAll is more aggressive.
      // TODO: b/454095852 - Remove this when
      // https://chromium-review.googlesource .com/c/chromium/src/+/7127962
      // lands on main
      ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();
      break;
    }
>>>>>>> 22869a70485 (Deprecate kSbEventTypeVerticalSync from Chrobalt (#8835))
    case kSbEventTypeScheduled:
    case kSbEventTypeLowMemory:
    case kSbEventTypeWindowSizeChanged:
      if (g_platform_event_source) {
        g_platform_event_source->HandleWindowSizeChangedEvent(event);
      }
      break;
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
    case kSbEventDateTimeConfigurationChanged:
      break;
  }
}

int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
