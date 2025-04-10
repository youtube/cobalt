// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/path_service.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/shell/browser/shell_paths.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/browser/android/mojo/cobalt_interface_registrar_android.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#endif

namespace cobalt {

CobaltBrowserMainParts::CobaltBrowserMainParts() = default;

CobaltBrowserMainParts::~CobaltBrowserMainParts() = default;

int CobaltBrowserMainParts::PreEarlyInitialization() {
  global_features_ = std::make_unique<GlobalFeatures>(
      std::move(experiment_config_), std::move(local_state_),
      std::move(metrics_services_manager_), metrics_services_manager_client_);
  return ShellBrowserMainParts::PreEarlyInitialization();
}

int CobaltBrowserMainParts::PreCreateThreads() {
  SetupMetrics();
  return ShellBrowserMainParts::PreCreateThreads();
}

int CobaltBrowserMainParts::PreMainMessageLoopRun() {
  StartMetricsRecording();
  return ShellBrowserMainParts::PreMainMessageLoopRun();
}

void CobaltBrowserMainParts::SetMetricsServices(
    std::unique_ptr<metrics_services_manager::MetricsServicesManager> manager,
    metrics_services_manager::MetricsServicesManagerClient* client) {
  metrics_services_manager_ = std::move(manager);
  metrics_services_manager_client_ =
      static_cast<CobaltMetricsServicesManagerClient*>(client);
}

void CobaltBrowserMainParts::SetupMetrics() {
  metrics::MetricsService* metrics = global_features_->metrics_service();
  metrics->InitializeMetricsRecordingState();
  DLOG(INFO) << "Cobalt Metrics Service initialized.";
}

void CobaltBrowserMainParts::StartMetricsRecording() {
  // This call kicks off the whole metric recording flow. It sets a timer and
  // periodically triggers a UMA payload to be handled by the logs uploader.
  global_features_->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  DLOG(INFO) << "Metrics Service is now running/recording.";
}

metrics::MetricsService* CobaltBrowserMainParts::GetMetricsService() {
  auto* metrics_services_manager = GetMetricsServicesManager();
  if (metrics_services_manager) {
    return metrics_services_manager->GetMetricsService();
  }
  return nullptr;
}

metrics_services_manager::MetricsServicesManager*
CobaltBrowserMainParts::GetMetricsServicesManager() {
  // TODO(b/372559349): Can I check for teardown here like Chrome does:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/browser_process_impl.cc;l=796;drc=87c3217dc3fec0f441b68f33d339b7f3a707b11d.
  if (!metrics_services_manager_) {
    auto client = std::make_unique<CobaltMetricsServicesManagerClient>(
        local_state_.get());
    metrics_services_manager_ =
        std::make_unique<metrics_services_manager::MetricsServicesManager>(
            std::move(client));
  }
  return metrics_services_manager_.get();
}

void CobaltBrowserMainParts::set_experiment_config(
    std::unique_ptr<PrefService> experiment_config) {
  experiment_config_ = std::move(experiment_config);
}

void CobaltBrowserMainParts::set_local_state(
    std::unique_ptr<PrefService> local_state) {
  local_state_ = std::move(local_state);
}

#if BUILDFLAG(IS_ANDROIDTV)
void CobaltBrowserMainParts::PostCreateThreads() {
  // TODO(cobalt, b/383301493): this looks like a reasonable stage at which to
  // register these interfaces and it seems to work. But we may want to
  // consider if there's a more suitable stage.
  RegisterCobaltJavaMojoInterfaces();
  ShellBrowserMainParts::PostCreateThreads();
}
#endif  // BUILDFLAG(IS_ANDROIDTV)

#if BUILDFLAG(IS_LINUX)
void CobaltBrowserMainParts::PostCreateMainMessageLoop() {
  // Set up crypt config. This needs to be done before anything starts the
  // network service, as the raw encryption key needs to be shared with the
  // network service for encrypted cookie storage.
  // Chrome OS does not need a crypt config as its user data directories are
  // already encrypted and none of the true encryption backends used by
  // desktop Linux are available on Chrome OS anyway.
  std::unique_ptr<os_crypt::Config> config =
      std::make_unique<os_crypt::Config>();
  // Forward the product name
  config->product_name = "Cobalt";
  // OSCrypt may target keyring, which requires calls from the main thread.
  config->main_thread_runner = content::GetUIThreadTaskRunner({});
  // OSCrypt can be disabled in a special settings file.
  config->should_use_preference = false;
  base::PathService::Get(content::SHELL_DIR_USER_DATA, &config->user_data_path);
  OSCrypt::SetConfig(std::move(config));
  ShellBrowserMainParts::PostCreateMainMessageLoop();
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace cobalt
