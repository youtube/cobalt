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

#include "cobalt/browser/cobalt_browser_main_parts.h"

#include <memory>

#include "base/path_service.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "base/android/memory_pressure_listener_android.h"
#include "cobalt/browser/android/mojo/cobalt_interface_registrar_android.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#endif

namespace cobalt {

CobaltBrowserMainParts::CobaltBrowserMainParts(bool is_visible)
    : ShellBrowserMainParts(is_visible) {}

int CobaltBrowserMainParts::PreCreateThreads() {
  SetupMetrics();
#if BUILDFLAG(IS_ANDROIDTV)
  base::android::MemoryPressureListenerAndroid::Initialize(
      base::android::AttachCurrentThread());
#endif
  return ShellBrowserMainParts::PreCreateThreads();
}

int CobaltBrowserMainParts::PreMainMessageLoopRun() {
  StartMetricsRecording();
  return ShellBrowserMainParts::PreMainMessageLoopRun();
}

void CobaltBrowserMainParts::PostDestroyThreads() {
  GlobalFeatures::GetInstance()->Shutdown();
  ShellBrowserMainParts::PostDestroyThreads();
}

void CobaltBrowserMainParts::SetupMetrics() {
  metrics::MetricsService* metrics =
      GlobalFeatures::GetInstance()->metrics_service();
  metrics->InitializeMetricsRecordingState();
  DLOG(INFO) << "Cobalt Metrics Service initialized.";
}

void CobaltBrowserMainParts::StartMetricsRecording() {
  // This call kicks off the whole metric recording flow. It sets a timer and
  // periodically triggers a UMA payload to be handled by the logs uploader.
  // Note, "may_upload" is always true but reporting itself can be disabled and
  // no uploads are sent.
  GlobalFeatures::GetInstance()
      ->metrics_services_manager()
      ->UpdateUploadPermissions(true);
  DLOG(INFO) << "Metrics Service is now running/recording.";
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
