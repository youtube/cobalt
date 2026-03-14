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

#include "cobalt/app/app_event_runner.h"

#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "cobalt/app/app_event_delegate.h"
#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/network_change_notifier_passive.h"

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/app/cobalt_switch_defaults_starboard.h"
#include "services/device/time_zone_monitor/time_zone_monitor_starboard.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#endif

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include <init_musl.h>
#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/browser/loader_app_metrics.h"
#endif
#endif

namespace cobalt {

namespace {
content::ContentMainRunner* GetContentMainRunner() {
  static base::NoDestructor<std::unique_ptr<content::ContentMainRunner>>
      main_runner{content::ContentMainRunner::Create()};
  return main_runner->get();
}
}  // namespace

class AppEventRunnerImpl : public AppEventRunner {
 public:
  AppEventRunnerImpl() = default;
  ~AppEventRunnerImpl() override = default;

  void InitializeSystem() override {
    exit_manager_ = std::make_unique<base::AtExitManager>();
  }

  void CreateMainDelegate(absl::optional<int64_t> startup_timestamp,
                          bool is_visible,
                          const char* initial_deep_link) override {
    content_main_delegate_ = std::make_unique<cobalt::CobaltMainDelegate>(
        startup_timestamp, false /* is_content_browsertests */, is_visible,
        initial_deep_link);
  }

  cobalt::CobaltMainDelegate* GetMainDelegate() override {
    return content_main_delegate_.get();
  }

  int Run(content::ContentMainParams params) override {
    main_runner_ = GetContentMainRunner();
    is_running_ = true;
    return content::ContentMain(std::move(params));
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
    // no longer recognizes the main thread as a valid TaskEnvironment.
    if (main_runner_) {
      main_runner_->Shutdown();
    }

    // Destroy only after main_runner_/ContentMainRunnerImpl is shutdown
    // as the delegate is used internally.
    content_main_delegate_.reset();
    exit_manager_.reset();
    is_running_ = false;
  }

  bool IsRunning() const override { return is_running_; }

  bool IsVisible() const override { return is_visible_; }

  void SetStateForTesting(bool is_running, bool is_visible) override {
    is_running_ = is_running;
    is_visible_ = is_visible;
  }

  void OnStart(const SbEvent* event) override {
    SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    if (IsRunning()) {
      if (data->link && data->link[0]) {
        LOG(WARNING) << "Received redundant "
                     << (event->type == kSbEventTypeStart ? "start" : "preload")
                     << " event with link after initialization. Treating as a "
                        "link event.";
        SbEvent link_event = {kSbEventTypeLink, event->timestamp,
                              const_cast<char*>(data->link)};
        OnLink(&link_event);
      }
      return;
    }
    CHECK(!IsRunning());
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
    init_musl();
#endif
    InitializeSystem();
#if BUILDFLAG(IS_STARBOARD)
    platform_event_source_ =
        std::make_unique<ui::PlatformEventSourceStarboard>();
#endif

    is_visible_ = event->type == kSbEventTypeStart;
    if (data) {
      RunInternal(event->timestamp, is_visible_, data->argument_count,
                  const_cast<const char**>(data->argument_values), data->link);
    } else {
      RunInternal(event->timestamp, is_visible_, 0, nullptr, nullptr);
    }

#if BUILDFLAG(USE_EVERGREEN)
    // Log Loader App Metrics.
    cobalt::browser::RecordLoaderAppMetrics();
#endif
  }

  void OnBlur() override {
    CHECK(IsRunning());
    CHECK(IsVisible());
    content::Shell::OnBlur();
#if BUILDFLAG(IS_STARBOARD)
    if (platform_event_source_) {
      platform_event_source_->DispatchFocusEvent(false);
    }
#else
    // Other platforms route events differently, e.g. on AndroidTV
    // focus is handled natively via Activity and View lifecycle callbacks
    // (e.g. CobaltActivity.onResume) which propagate directly to Chromium's
    // WindowAndroid.
#endif
  }

  void OnFocus() override {
    CHECK(IsRunning());
    CHECK(IsVisible());
    content::Shell::OnFocus();
#if BUILDFLAG(IS_STARBOARD)
    if (platform_event_source_) {
      platform_event_source_->DispatchFocusEvent(true);
    }
#else
    // On Android, focus is handled natively via Activity and View lifecycle
    // callbacks (e.g. CobaltActivity.onResume) which propagate directly to
    // Chromium's WindowAndroid.
#endif
  }

  void OnConceal() override {
    CHECK(IsRunning());
    CHECK(IsVisible());
    content::Shell::OnConceal();
    is_visible_ = false;
  }

  void OnReveal() override {
    CHECK(IsRunning());
    CHECK(!IsVisible());
    content::Shell::OnReveal();
    is_visible_ = true;
  }

  void OnFreeze() override {
    CHECK(IsRunning());
    CHECK(!IsVisible());
    content::Shell::OnFreeze();
  }

  void OnUnfreeze() override {
    CHECK(IsRunning());
    CHECK(!IsVisible());
    content::Shell::OnUnfreeze();
  }

  void OnStop() override {
    if (!IsRunning()) {
      return;
    }
    CHECK(!IsVisible());
    content::Shell::OnStop();
    ShutDown();
#if BUILDFLAG(IS_STARBOARD)
    platform_event_source_.reset();
#endif
  }

  void OnInput(const SbEvent* event) override {
#if BUILDFLAG(IS_STARBOARD)
    if (platform_event_source_) {
      platform_event_source_->HandleEvent(event);
    }
#else
    // On Android, input is handled natively via standard View event listeners
    // (e.g. CobaltActivity.onKeyDown) which are converted directly to
    // Chromium Motion/KeyEvents.
#endif
  }

  void OnLink(const SbEvent* event) override {
    auto link = static_cast<const char*>(event->data);
    if (link) {
      cobalt::browser::DeepLinkManager::GetInstance()->OnDeepLink(link);
    }
  }

  void OnLowMemory(const SbEvent* event) override {
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

    // Chromium internally calls Reclaim/ReclaimNormal at regular interval
    // to claim free memory. Using ReclaimAll is more aggressive.
    ::partition_alloc::MemoryReclaimer::Instance()->ReclaimAll();

    if (event->data) {
      auto mem_cb = reinterpret_cast<SbEventCallback>(event->data);
      mem_cb(nullptr);
    }
  }

  void OnAccessibilityTextToSpeechSettingsChanged(
      const SbEvent* event) override {
    if (event->data) {
      auto* enabled = static_cast<const bool*>(event->data);
      cobalt::browser::H5vccAccessibilityManager::GetInstance()
          ->OnTextToSpeechStateChanged(*enabled);
    }
  }

  void OnWindowSizeChanged(const SbEvent* event) override {
#if BUILDFLAG(IS_STARBOARD)
    if (platform_event_source_) {
      platform_event_source_->HandleWindowSizeChangedEvent(event);
    }
#else
    // On Android, window size changes are handled natively via View
    // onSizeChanged/onConfigurationChanged which update Chromium's
    // WindowAndroid and viewport directly.
#endif
  }

  void OnOsNetworkConnectedDisconnected(const SbEvent* event) override {
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
  }

  void OnDateTimeConfigurationChanged(const SbEvent* event) override {
#if BUILDFLAG(IS_STARBOARD)
    device::NotifyTimeZoneChangeStarboard();
#else
    // Android handles network and time zone changes natively via
    // ConnectivityManager and TimeZoneChangeReceiver.
#endif
  }

 private:
  int RunInternal(absl::optional<int64_t> startup_timestamp,
                  bool is_visible,
                  int argc,
                  const char** argv,
                  const char* initial_deep_link) {
    CreateMainDelegate(startup_timestamp, is_visible, initial_deep_link);

    content::ContentMainParams params(GetMainDelegate());

#if BUILDFLAG(IS_STARBOARD)
    cobalt::CommandLinePreprocessor init_cmd_line(argc, argv);
    const auto& init_argv = init_cmd_line.argv();

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
#endif

    std::vector<const char*> args;
    for (const auto& arg : init_argv) {
      args.push_back(arg.c_str());
    }
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

    is_running_ = true;
    return Run(std::move(params));
  }

  std::unique_ptr<base::AtExitManager> exit_manager_;
  content::ContentMainRunner* main_runner_ = nullptr;
  std::unique_ptr<cobalt::CobaltMainDelegate> content_main_delegate_;
  bool is_visible_ = false;
  bool is_running_ = false;

#if BUILDFLAG(IS_STARBOARD)
  std::unique_ptr<ui::PlatformEventSourceStarboard> platform_event_source_;
#endif
};

std::unique_ptr<AppEventRunner> AppEventRunner::Create() {
  return std::make_unique<AppEventRunnerImpl>();
}

}  // namespace cobalt
