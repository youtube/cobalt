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

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
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
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "cobalt/shell/common/shell_switches.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "services/device/time_zone_monitor/time_zone_monitor_starboard.h"
#include "starboard/event.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include <init_musl.h>
#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/browser/loader_app_metrics.h"
#endif
#endif

using ui::PlatformEventSourceStarboard;

namespace {

static base::AtExitManager* g_exit_manager = nullptr;
static cobalt::CobaltMainDelegate* g_content_main_delegate = nullptr;
static ui::PlatformEventSourceStarboard* g_platform_event_source = nullptr;

content::ContentMainRunner* GetContentMainRunner() {
  static base::NoDestructor<std::unique_ptr<content::ContentMainRunner>> runner{
      content::ContentMainRunner::Create()};
  if (!g_exit_manager) {
    return nullptr;
  }
  return runner->get();
}

}  // namespace

int InitCobalt(int argc, const char** argv, const char* initial_deep_link) {
  if (g_exit_manager) {
    return 0;
  }

  LOG(INFO) << "InitCobalt called with " << argc << " args:";
  for (int i = 0; i < argc; ++i) {
    LOG(INFO) << "Arg " << i << ": " << argv[i];
  }

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
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

  g_exit_manager = new base::AtExitManager();
  g_content_main_delegate = new cobalt::CobaltMainDelegate();
  g_platform_event_source = new PlatformEventSourceStarboard();

  base::AtExitManager::RegisterTask(base::BindOnce([]() {
    delete g_platform_event_source;
    g_platform_event_source = nullptr;
  }));

  base::AtExitManager::RegisterTask(base::BindOnce([]() {
    delete g_content_main_delegate;
    g_content_main_delegate = nullptr;
  }));

  base::AtExitManager::RegisterTask(base::BindOnce([]() {
    if (GetContentMainRunner()) {
      GetContentMainRunner()->Shutdown();
    }
  }));

  base::AtExitManager::RegisterTask(base::BindOnce([]() {
    if (g_content_main_delegate) {
      g_content_main_delegate->Shutdown();
    }
  }));

  content::ContentMainParams params(g_content_main_delegate);

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

#if BUILDFLAG(USE_EVERGREEN)
  // Log Loader App Metrics.
  cobalt::browser::RecordLoaderAppMetrics();
#endif

  return RunContentProcess(std::move(params), GetContentMainRunner());
}

void FinalizeCobalt() {
  if (!g_exit_manager) {
    return;
  }

  delete g_exit_manager;
  g_exit_manager = nullptr;
}

void SbEventHandle(const SbEvent* event) {
  if (!g_exit_manager && event->type != kSbEventTypeStart &&
      event->type != kSbEventTypePreload) {
    // Treat as an implicit preload if an event is received before a start or
    // preload. This should never happen, but we handle it here as a failsafe.
    LOG(WARNING) << "SbEventHandle: Received event " << event->type
                 << " before initialization. Treating as implicit preload.";
    SbEvent start_event = {kSbEventTypePreload, 0, nullptr};
    SbEventHandle(&start_event);
  }

  switch (event->type) {
    case kSbEventTypeStart: {
      if (g_exit_manager) {
        content::Shell::OnFocus();
        break;
      }
      ABSL_FALLTHROUGH_INTENDED;
    }
    case kSbEventTypePreload: {
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
      init_musl();
#endif
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      if (data) {
        InitCobalt(data->argument_count,
                   const_cast<const char**>(data->argument_values), data->link);
      } else {
        // Handle implicit preloads where no event data was provided by using
        // the command-line parameters already initialized for the process.
        LOG(WARNING) << "SbEventHandle: Implicit preload using "
                        "command-line parameters.";
        if (base::CommandLine::InitializedForCurrentProcess()) {
          const auto& argv = base::CommandLine::ForCurrentProcess()->argv();
          std::vector<const char*> argv_ptrs;
          for (const auto& arg : argv) {
            argv_ptrs.push_back(arg.c_str());
          }
          InitCobalt(argv_ptrs.size(), argv_ptrs.data(), nullptr);
        } else {
          InitCobalt(0, nullptr, nullptr);
        }
      }
      if (event->type == kSbEventTypePreload) {
        content::Shell::OnUnfreeze();
      } else {
        content::Shell::OnFocus();
      }
      break;
    }
    case kSbEventTypeStop: {
      if (!g_exit_manager) {
        break;
      }

      content::Shell::OnStop();
      content::Shell::Shutdown();
      break;
    }
    case kSbEventTypeBlur:
      content::Shell::OnBlur();
      CHECK(g_platform_event_source);
      g_platform_event_source->HandleFocusEvent(event);
      break;
    case kSbEventTypeFocus:
      content::Shell::OnFocus();
      CHECK(g_platform_event_source);
      g_platform_event_source->HandleFocusEvent(event);
      break;
    case kSbEventTypeConceal:
      content::Shell::OnConceal();
      break;
    case kSbEventTypeReveal:
      content::Shell::OnReveal();
      break;
    case kSbEventTypeFreeze:
      content::Shell::OnFreeze();
      break;
    case kSbEventTypeUnfreeze:
      content::Shell::OnUnfreeze();
      break;
    case kSbEventTypeInput:
      CHECK(g_platform_event_source);
      g_platform_event_source->HandleEvent(event);
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
    case kSbEventTypeScheduled:
    case kSbEventTypeWindowSizeChanged:
      CHECK(g_platform_event_source);
      g_platform_event_source->HandleWindowSizeChangedEvent(event);
      break;
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
    case kSbEventDateTimeConfigurationChanged:
      device::NotifyTimeZoneChangeStarboard();
      break;
  }
}

#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
int main(int argc, char** argv) {
  int result = SbRunStarboardMain(argc, argv, SbEventHandle);
  FinalizeCobalt();
  CHECK(!g_exit_manager);
  return result;
}
#endif
