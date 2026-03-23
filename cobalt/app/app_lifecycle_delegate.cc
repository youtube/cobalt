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

#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/src/partition_alloc/memory_reclaimer.h"
#include "base/at_exit.h"
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
#include "content/public/browser/network_service_instance.h"
#include "net/base/network_change_notifier_passive.h"

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/app/cobalt_switch_defaults.h"
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

class DefaultAppLifecycleRunner : public cobalt::AppLifecycleRunner {
 public:
  DefaultAppLifecycleRunner() = default;
  ~DefaultAppLifecycleRunner() override = default;

  void InitializeSystem() override {
    exit_manager_ = std::make_unique<base::AtExitManager>();
  }

  void CreateMainDelegate(bool is_visible,
                          const char* initial_deep_link) override {
    content_main_delegate_ = std::make_unique<cobalt::CobaltMainDelegate>(
        initial_deep_link, false /* is_content_browsertests */, is_visible);
  }

  cobalt::CobaltMainDelegate* GetMainDelegate() override {
    return content_main_delegate_.get();
  }

  int Run(content::ContentMainParams params) override {
    if (main_runner_) {
      LOG(WARNING) << "AppLifecycleRunner::Run called multiple times.";
      return -1;
    }
    main_runner_ = GetContentMainRunner();
    return RunContentProcess(std::move(params), main_runner_);
  }

  void ShutDown() override {
    content::Shell::Shutdown();

    if (content_main_delegate_) {
      content_main_delegate_->Shutdown();
    }

    // Must be called after content_main_delegate_->Shutdown() to prevent
    // unregistering the main thread from the SequenceManager which
    // happens with the destruction of the BrowserTaskExecutor. If the order
    // is reversed the SequenceManager complains (fails DCHECK) that the
    // ContentMainRunnerImpl::Shutdown() is happening on the wrong thread as
    // it no longer recognizes the main thread as a valid TaskEnvironment.
    if (main_runner_) {
      main_runner_->Shutdown();
    }

    // Destroy only after main_runner_/ContentMainRunnerImpl is shutdown
    // as the delegate is used internally.
    content_main_delegate_.reset();

    // We intentionally do not null main_runner_ here to enforce the one-shot
    // rule: once stopped, the process cannot be re-started.
    exit_manager_.reset();
  }

  bool IsRunning() const override { return main_runner_ != nullptr; }

 private:
  std::unique_ptr<base::AtExitManager> exit_manager_;
  content::ContentMainRunner* main_runner_ = nullptr;
  std::unique_ptr<cobalt::CobaltMainDelegate> content_main_delegate_;
};

}  // namespace

namespace cobalt {

AppLifecycleDelegate::AppLifecycleDelegate(
    std::unique_ptr<AppLifecycleRunner> runner)
    : runner_(std::move(runner)) {
  if (!runner_) {
    runner_ = std::make_unique<DefaultAppLifecycleRunner>();
  }
}

AppLifecycleDelegate::~AppLifecycleDelegate() = default;

bool AppLifecycleDelegate::IsRunning() const {
  return runner_->IsRunning();
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
      content::Shell::OnBlur();
      {
        auto* client = cobalt::CobaltContentBrowserClient::Get();
        if (client) {
          client->DispatchBlur();
        }
      }
#if BUILDFLAG(IS_STARBOARD)
      if (platform_event_source_) {
        platform_event_source_->HandleFocusEvent(event);
      }
#endif
      break;
    case kSbEventTypeFocus:
      content::Shell::OnFocus();
      {
        auto* client = cobalt::CobaltContentBrowserClient::Get();
        if (client) {
          client->DispatchFocus();
        }
      }
#if BUILDFLAG(IS_STARBOARD)
      if (platform_event_source_) {
        platform_event_source_->HandleFocusEvent(event);
      }
#endif
      break;
    case kSbEventTypeConceal:
      content::Shell::OnConceal();
      break;
    case kSbEventTypeReveal:
      content::Shell::OnReveal();
      break;
    case kSbEventTypeFreeze:
      content::Shell::OnFreeze();
      {
        auto* client = cobalt::CobaltContentBrowserClient::Get();
        if (client) {
          client->FlushCookiesAndLocalStorage();
        }
      }
      break;
    case kSbEventTypeUnfreeze:
      content::Shell::OnUnfreeze();
      break;
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
      // https://chromium-review.googlesource.com/c/chromium/src/+/7127962
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
    case kSbEventTypeOsNetworkConnected: {
#if BUILDFLAG(IS_STARBOARD)
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
#endif
      break;
    }
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
  runner_->InitializeSystem();
#if BUILDFLAG(IS_STARBOARD)
  platform_event_source_ = std::make_unique<ui::PlatformEventSourceStarboard>();
#endif

  bool is_visible = event->type == kSbEventTypeStart;
  if (data) {
    Run(is_visible, data->argument_count,
        const_cast<const char**>(data->argument_values), data->link);
  } else {
    Run(is_visible, 0, nullptr, nullptr);
  }

#if BUILDFLAG(USE_EVERGREEN)
  // Log Loader App Metrics.
  cobalt::browser::RecordLoaderAppMetrics();
#endif
}

void AppLifecycleDelegate::OnStop(const SbEvent* event) {
  if (!runner_->IsRunning()) {
    return;
  }
  content::Shell::OnStop();
  runner_->ShutDown();

#if BUILDFLAG(IS_STARBOARD)
  platform_event_source_.reset();
#endif
}

int AppLifecycleDelegate::Run(bool is_visible,
                              int argc,
                              const char** argv,
                              const char* initial_deep_link) {
  runner_->CreateMainDelegate(is_visible, initial_deep_link);

  content::ContentMainParams params(runner_->GetMainDelegate());

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
  if ((argc >= 1 && !strcmp(argv[0], "/proc/self/exe")) ||
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

  return runner_->Run(std::move(params));
}
}  // namespace cobalt
