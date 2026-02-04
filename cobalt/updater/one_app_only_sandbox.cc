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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This is a test app for Evergreen that only works when it loads the
// evergreen-cert-test.html page.

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "cobalt/app/cobalt_switch_defaults_starboard.h"
#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "starboard/event.h"
#include "starboard/loader_app/app_key.h"
#include "starboard/system.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

#include <init_musl.h>

using ui::PlatformEventSourceStarboard;

namespace {

const char kMainAppKey[] = "aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=";
// Use the html instead of url or app key to target the Evergreen cert test
// page, because there are various urls to launch the test page.
const char kEvergreenCertTestHtml[] = "evergreen-cert-test.html";

bool is_target_app = false;

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
  content::ContentMainParams params(g_content_main_delegate);

  // Preprocess the raw command line arguments with the defaults expected by
  // Cobalt.
  cobalt::CommandLinePreprocessor init_cmd_line(argc, argv);
  const auto& init_argv = init_cmd_line.argv();

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  logging::SetMinLogLevel(logging::LOGGING_FATAL);
#endif

  std::stringstream ss;
  std::vector<const char*> args;
  for (const auto& arg : init_argv) {
    args.push_back(arg.c_str());
    ss << " " << arg;
  }
  LOG(INFO) << "Parsed command line string:" << ss.str();

  if (initial_deep_link) {
    auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
    manager->set_deep_link(initial_deep_link);
  }

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
  if (!is_target_app && (event->type == kSbEventTypePreload ||
                         event->type == kSbEventTypeStart)) {
    const SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    const base::CommandLine command_line(
        data->argument_count, const_cast<const char**>(data->argument_values));

    if (command_line.HasSwitch(cobalt::switches::kInitialURL)) {
      std::string url =
          command_line.GetSwitchValueASCII(cobalt::switches::kInitialURL);
      if (loader_app::GetAppKey(url) != kMainAppKey &&
          url.find(kEvergreenCertTestHtml) == std::string::npos) {
        // If the app is not the main app nor Evergreen cert test page, stop the
        // app.
        SbSystemRequestStop(0);
        return;
      }
    }

    is_target_app = true;
  }

  switch (event->type) {
    case kSbEventTypePreload: {
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
      init_musl();
#endif
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      g_exit_manager = new base::AtExitManager();
      g_content_main_delegate = new cobalt::CobaltMainDelegate();
      g_platform_event_source = new PlatformEventSourceStarboard();
      InitCobalt(data->argument_count,
                 const_cast<const char**>(data->argument_values), data->link);

      break;
    }
    case kSbEventTypeStart: {
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
      init_musl();
#endif
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      g_exit_manager = new base::AtExitManager();
      g_content_main_delegate = new cobalt::CobaltMainDelegate();
      g_platform_event_source = new PlatformEventSourceStarboard();
      InitCobalt(data->argument_count,
                 const_cast<const char**>(data->argument_values), data->link);

      break;
    }
    case kSbEventTypeStop: {
      if (g_content_main_delegate) {
        content::Shell::Shutdown();

        g_content_main_delegate->Shutdown();

        GetContentMainRunner()->Shutdown();

        delete g_content_main_delegate;
        g_content_main_delegate = nullptr;
      }

      if (g_platform_event_source) {
        delete g_platform_event_source;
        g_platform_event_source = nullptr;
      }

      if (g_exit_manager) {
        delete g_exit_manager;
        g_exit_manager = nullptr;
      }
      break;
    }
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
      if (g_platform_event_source) {
        g_platform_event_source->HandleFocusEvent(event);
      }
      break;
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
    case kSbEventTypeLink: {
      auto link = static_cast<const char*>(event->data);
      auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
      if (link) {
        manager->OnDeepLink(link);
      }
      break;
    }
    case kSbEventTypeLowMemory: {
      base::MemoryPressureListener::NotifyMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

      // Chromium internally calls Reclaim/ReclaimNormal at regular interval
      // to claim free memory. Using ReclaimAll is more aggressive.
      ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();

      if (event->data) {
        auto mem_cb = reinterpret_cast<SbEventCallback>(event->data);
        mem_cb(nullptr);
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
    case kSbEventTypeVerticalSync:
    case kSbEventTypeScheduled:
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
