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

#include "cobalt/app/app_lifecycle_delegate.h"

#include <unistd.h>

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/src/partition_alloc/memory_reclaimer.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "cobalt/browser/cobalt_content_browser_client.h"
#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/app/cobalt_switch_defaults.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/network_change_notifier_passive.h"
#include "services/device/time_zone_monitor/time_zone_monitor_starboard.h"
#endif

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include <init_musl.h>
#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/browser/loader_app_metrics.h"
#endif
#endif

namespace {
content::ContentMainRunner* GetContentMainRunner() {
  static base::NoDestructor<std::unique_ptr<content::ContentMainRunner>>
      main_runner{content::ContentMainRunner::Create()};
  return main_runner->get();
}
}  // namespace

namespace cobalt {

AppLifecycleDelegate::AppLifecycleDelegate() = default;

AppLifecycleDelegate::~AppLifecycleDelegate() = default;

bool AppLifecycleDelegate::IsRunning() const {
  return main_runner_ != nullptr;
}

void AppLifecycleDelegate::HandleEvent(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypePreload:
    case kSbEventTypeStart:
      OnStart(event);
      break;
    case kSbEventTypeStop:
      OnStop(event);
      break;
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
#if BUILDFLAG(IS_STARBOARD)
    {
      auto* client = cobalt::CobaltContentBrowserClient::Get();
      if (client) {
        if (event->type == kSbEventTypeBlur) {
          client->DispatchBlur();
        } else {
          client->DispatchFocus();
        }
      }
    }
      if (platform_event_source_) {
        platform_event_source_->HandleFocusEvent(event);
      }
#endif
      break;
    case kSbEventTypeConceal:
      break;
    case kSbEventTypeReveal:
      content::Shell::OnReveal();
      break;
    case kSbEventTypeFreeze: {
      auto* client = cobalt::CobaltContentBrowserClient::Get();
      if (client) {
        client->FlushCookiesAndLocalStorage();
      }
    } break;
    case kSbEventTypeUnfreeze:
    case kSbEventTypeInput:
#if BUILDFLAG(IS_STARBOARD)
      if (platform_event_source_) {
        platform_event_source_->HandleEvent(event);
      }
#endif
      break;
    case kSbEventTypeLink: {
      auto link = static_cast<const char*>(event->data);
      if (link) {
        cobalt::browser::DeepLinkManager::GetInstance()->OnDeepLink(link);
      }
      break;
    }
    case kSbEventTypeLowMemory: {
      // Send a one-time critical memory pressure signal to ask
      // other components to release memory.
      base::MemoryPressureListener::NotifyMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
      LOG(INFO) << "Firing a critical memory pressure signal to reduce memory "
                   "burden.";

      // Chromium internally calls Reclaim/ReclaimNormal at regular interval
      // to claim free memory. Using ReclaimAll is more aggressive.
      // TODO: b/454095852 - Remove this when
      // https://chromium-review.googlesource .com/c/chromium/src/+/7127962
      // lands on main
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
#if BUILDFLAG(IS_STARBOARD)
      if (platform_event_source_) {
        platform_event_source_->HandleWindowSizeChangedEvent(event);
      }
#endif
      break;
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
#if BUILDFLAG(IS_STARBOARD)
    {
      auto* notifier = content::GetNetworkChangeNotifier();
      if (notifier) {
        auto* passive_notifier =
            static_cast<net::NetworkChangeNotifierPassive*>(notifier);
        net::NetworkChangeNotifier::ConnectionType type =
            event->type == kSbEventTypeOsNetworkConnected
                ? net::NetworkChangeNotifier::CONNECTION_UNKNOWN
                : net::NetworkChangeNotifier::CONNECTION_NONE;
        net::NetworkChangeNotifier::ConnectionSubtype subtype =
            event->type == kSbEventTypeOsNetworkConnected
                ? net::NetworkChangeNotifier::SUBTYPE_UNKNOWN
                : net::NetworkChangeNotifier::SUBTYPE_NONE;
        passive_notifier->OnConnectionChanged(type);
        passive_notifier->OnConnectionSubtypeChanged(type, subtype);
      }
    }
#endif
    break;
    case kSbEventDateTimeConfigurationChanged:
#if BUILDFLAG(IS_STARBOARD)
      device::NotifyTimeZoneChangeStarboard();
#endif
      break;
  }
}

void AppLifecycleDelegate::OnStart(const SbEvent* event) {
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  init_musl();
#endif
  SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
  exit_manager_ = std::make_unique<base::AtExitManager>();
#if BUILDFLAG(IS_STARBOARD)
  platform_event_source_ = std::make_unique<ui::PlatformEventSourceStarboard>();
#endif

  Run(event->type == kSbEventTypeStart, data->argument_count,
      const_cast<const char**>(data->argument_values), data->link);

#if BUILDFLAG(USE_EVERGREEN)
  // Log Loader App Metrics.
  cobalt::browser::RecordLoaderAppMetrics();
#endif
}

void AppLifecycleDelegate::OnStop(const SbEvent* event) {
  content::Shell::Shutdown();

  if (main_runner_) {
    main_runner_->Shutdown();
  }
  main_runner_ = nullptr;

  if (content_main_delegate_) {
    content_main_delegate_->Shutdown();
  }
  content_main_delegate_.reset();

#if BUILDFLAG(IS_STARBOARD)
  platform_event_source_.reset();
#endif
  exit_manager_.reset();
}

int AppLifecycleDelegate::Run(bool is_visible,
                              int argc,
                              const char** argv,
                              const char* initial_deep_link) {
  main_runner_ = GetContentMainRunner();

  content_main_delegate_ = std::make_unique<cobalt::CobaltMainDelegate>(
      initial_deep_link, false /* is_content_browsertests */, is_visible);

  content::ContentMainParams params(content_main_delegate_.get());

#if BUILDFLAG(IS_STARBOARD)
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
#endif

  if (initial_deep_link) {
    auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
    manager->set_deep_link(initial_deep_link);
  }

  // This expression exists to ensure that we apply the argument overrides
  // only on the main process, not on spawned processes such as the zygote.
#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_STARBOARD)
  if ((!strcmp(argv[0], "/proc/self/exe")) ||
      ((argc >= 2) && !strcmp(argv[1], "--type=zygote"))) {
    params.argc = argc;
    params.argv = argv;
  } else {
    params.argc = args.size();
    params.argv = args.data();
  }
#else
  params.argc = argc;
  params.argv = argv;
#endif
#endif

  return RunContentProcess(std::move(params), main_runner_);
}
}  // namespace cobalt
