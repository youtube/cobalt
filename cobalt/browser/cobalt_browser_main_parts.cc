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

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "base/android/memory_pressure_listener_android.h"
#include "cobalt/browser/android/mojo/cobalt_interface_registrar_android.h"
#else
#include "cobalt/browser/cobalt_content_browser_client.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#endif

namespace cobalt {

namespace {

void InitializeBrowserMemoryInstrumentationClient() {
  if (memory_instrumentation::MemoryInstrumentation::GetInstance()) {
    return;
  }

  auto task_runner = base::trace_event::MemoryDumpManager::GetInstance()
                         ->GetDumpThreadTaskRunner();
  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&InitializeBrowserMemoryInstrumentationClient));
    return;
  }

  if (memory_instrumentation::MemoryInstrumentation::GetInstance()) {
    return;
  }

  // Register the browser process as a memory-instrumentation client.
  // This replicates content::InitializeBrowserMemoryInstrumentationClient()
  // while avoiding unauthorized header includes.
  mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator;
  mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess> process;
  auto process_receiver = process.InitWithNewPipeAndPassReceiver();
  content::GetMemoryInstrumentationRegistry()->RegisterClientProcess(
      coordinator.InitWithNewPipeAndPassReceiver(), std::move(process),
      memory_instrumentation::mojom::ProcessType::BROWSER,
      base::GetCurrentProcId(), /*service_name=*/absl::nullopt);
  memory_instrumentation::ClientProcessImpl::CreateInstance(
      std::move(process_receiver), std::move(coordinator),
      /*is_browser_process=*/true);
}

}  // namespace

void CobaltBrowserMainParts::InitializeMessageLoopContext() {
  // On Android, we completely defer WebContents creation until the Java layer
  // invokes ShellManager.launchShell() which reaches C++ via JNI.
  // On Linux, we might need the base behavior.
  // go/chrobalt-pre-initialization-storage-migration-pipeline
#if !BUILDFLAG(IS_ANDROID)
  auto create_window_task = base::BindOnce(
      &CobaltBrowserMainParts::CreateWindowWithMigrationStatus,
      base::Unretained(this), GetStartupURL(), deep_link(), browser_context());
  PostOrRunIfStorageMigrationFinished(std::move(create_window_task));
#endif
}

void CobaltBrowserMainParts::CreateWindowWithMigrationStatus(
    GURL url,
    std::string deeplink_url,
    content::ShellBrowserContext* browser_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string migration_status = cobalt::migrate_storage_record::
      MigrationManager::GetMigrationStatusUrlParameter();
  if (!migration_status.empty() && url.is_valid()) {
    GURL::Replacements replacements;
    std::string query = url.query();
    if (!query.empty()) {
      query += "&";
    }
    query += migration_status;
    replacements.SetQueryStr(query);
    url = url.ReplaceComponents(replacements);
    LOG(INFO)
        << "Storage migration status telemetry injected into startup URL: "
        << url.spec();
  }

  content::Shell::CreateNewWindow(browser_context, url, nullptr, gfx::Size(),
                                  ::switches::ShouldCreateSplashScreen(),
                                  deeplink_url);
}

CobaltBrowserMainParts::CobaltBrowserMainParts(bool is_visible,
                                               const std::string& deep_link)
    : ShellBrowserMainParts(is_visible, deep_link) {}

int CobaltBrowserMainParts::PreCreateThreads() {
  SetupMetrics();

  InitializeBrowserMemoryInstrumentationClient();

#if BUILDFLAG(IS_ANDROIDTV)
  base::android::MemoryPressureListenerAndroid::Initialize(
      base::android::AttachCurrentThread());
#endif
  return ShellBrowserMainParts::PreCreateThreads();
}

int CobaltBrowserMainParts::PreMainMessageLoopRun() {
  StartMetricsRecording();

#if !BUILDFLAG(IS_ANDROIDTV)
  auto* client = CobaltContentBrowserClient::Get();
  CHECK(client) << "CobaltContentBrowserClient::Get() returned NULL in "
                << "PreMainMessageLoopRun!";
  client->SetUserAgentCrashAnnotation();

#endif  // !BUILDFLAG(IS_ANDROIDTV)

  int result = ShellBrowserMainParts::PreMainMessageLoopRun();

  if (result != 0) {
    LOG(ERROR) << "PreMainMessageLoopRun failed with result: " << result
               << ". Aborting storage migration.";
    return result;
  }

  StartStorageMigration();

  return result;
}

void CobaltBrowserMainParts::StartStorageMigration() {
  LOG(INFO) << "CobaltBrowserMainParts::StartStorageMigration started.";
  content::StoragePartition* partition =
      browser_context()->GetDefaultStoragePartition();

  DCHECK(partition);
  cobalt::migrate_storage_record::MigrationManager::RunMigration(
      partition, base::BindOnce(&CobaltBrowserMainParts::OnMigrationComplete,
                                weak_ptr_factory_.GetWeakPtr()));
}

void CobaltBrowserMainParts::OnMigrationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Migration complete. Proceeding with deferred launchShell.";

  migration_finished_ = true;
  if (pending_task_) {
    std::move(pending_task_).Run();
  }
}

void CobaltBrowserMainParts::PostOrRunIfStorageMigrationFinished(
    base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sequence_checker_.CalledOnValidSequence());
  if (!migration_finished_) {
    LOG(INFO) << "Deferring launchShell until migration finishes.";
    CHECK(!pending_task_) << "Only one storage migration task is supported.";
    pending_task_ = std::move(task);
  } else {
    LOG(INFO) << "Migration already finished, running launchShell "
                 "immediately.";
    std::move(task).Run();
  }
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
