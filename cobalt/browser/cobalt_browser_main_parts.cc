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
#include "base/run_loop.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "base/android/memory_pressure_listener_android.h"
#include "cobalt/browser/android/mojo/cobalt_interface_registrar_android.h"
#include "starboard/android/shared/starboard_bridge.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#endif

namespace cobalt {

int CobaltBrowserMainParts::PreCreateThreads() {
#if BUILDFLAG(IS_ANDROIDTV)
  starboard::android::shared::StarboardBridge::GetInstance()
      ->SetStartupMilestone(17);
#endif
  SetupMetrics();
#if BUILDFLAG(IS_ANDROIDTV)
  base::android::MemoryPressureListenerAndroid::Initialize(
      base::android::AttachCurrentThread());
#endif
  return ShellBrowserMainParts::PreCreateThreads();
}

int CobaltBrowserMainParts::PreMainMessageLoopRun() {
  StartMetricsRecording();
  int result = ShellBrowserMainParts::PreMainMessageLoopRun();

  LOG(INFO) << "ColinL: CobaltBrowserMainParts::PreMainMessageLoopRun started.";

  // FIX: Perform migration before the first URL load happens.
  // Using the static accessor content::Shell::windows() as GetShells() is not
  // available on context.
  if (!content::Shell::windows().empty()) {
    LOG(INFO)
        << "ColinL: Shell window detected. Preparing for storage migration.";
    content::WebContents* web_contents =
        content::Shell::windows()[0]->web_contents();

    // 1. Kill any default navigation that might have started automatically.
    web_contents->Stop();

    // 2. Block the UI thread until migration completes.
    // base::RunLoop run_loop;
    migrate_storage_record::MigrationManager::EnsureMigrationDone(
        web_contents, base::DoNothing());
    // run_loop.Run();

    // 3. Perform the actual navigation now that storage is ready.
    // FIX: Using explicit GURL constructor call to satisfy 'explicit' keyword
    // in gurl.h and avoiding vexing parse by using copy-initialization from a
    // temporary. GURL initial_url =
    // GURL(::cobalt::switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
    // content::NavigationController::LoadURLParams params(initial_url);

    // // Use AUTO_TOPLEVEL as this is the automated "Home Page" load of the
    // app.
    // // Standard Chromium apps use this for the initial startup navigation.
    // params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
    // web_contents->GetController().LoadURLWithParams(params);
  }

  return result;
}

void CobaltBrowserMainParts::PostMainMessageLoopRun() {
  if (browser_context()) {
    content::StoragePartition* partition =
        browser_context()->GetDefaultStoragePartition();
    if (partition) {
      base::RunLoop run_loop;
      partition->GetCookieManagerForBrowserProcess()->FlushCookieStore(
          run_loop.QuitClosure());
      run_loop.Run();
      partition->Flush();
    }
  }
  ShellBrowserMainParts::PostMainMessageLoopRun();
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
