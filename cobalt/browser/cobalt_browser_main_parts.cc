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

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

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

CobaltBrowserMainParts::CobaltBrowserMainParts(const std::string& deep_link)
    : ShellBrowserMainParts(deep_link) {}

int CobaltBrowserMainParts::PreCreateThreads() {
#if BUILDFLAG(IS_ANDROIDTV)
  starboard::android::shared::StarboardBridge::GetInstance()
      ->SetStartupMilestone(17);
#endif
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

  if (base::FeatureList::IsEnabled(features::kAsyncDnsAndDoH)) {
    ConfigureAsyncDnsAndDoH();
  }

  return ShellBrowserMainParts::PreMainMessageLoopRun();
}

void CobaltBrowserMainParts::ConfigureAsyncDnsAndDoH() {
  // Note: The built-in Async DNS client and therefore DoH is not supported
  // on iOS. On iOS, the compilation of the built-in DNS resolver in
  // Chromium's network stack is disabled due to apple store regulations.
  if (auto* network_service = content::GetNetworkService()) {
    std::vector<net::DnsOverHttpsServerConfig> doh_servers;
    // Collect all globally available, enabled DoH providers.
    // This provides a fallback mechanism: if the first server is unreachable,
    // the network stack will try the next ones.
    for (const net::DohProviderEntry* provider :
         net::DohProviderEntry::GetList()) {
      if (provider->display_globally &&
          base::FeatureList::IsEnabled(provider->feature)) {
        doh_servers.push_back(provider->doh_server_config);
      }
    }

    absl::optional<net::DnsOverHttpsConfig> doh_config;
    if (!doh_servers.empty()) {
      doh_config = net::DnsOverHttpsConfig(std::move(doh_servers));
    }

    // kAutomatic means DoH lookups will be performed first if available.
    // If the DoH server is unreachable, it will gracefully fall back to
    // using the standard, unencrypted DNS resolution.
    // If the provided URL is empty or invalid, we turn DoH off entirely.
    auto secure_dns_mode = doh_config && !doh_config->servers().empty()
                               ? net::SecureDnsMode::kAutomatic
                               : net::SecureDnsMode::kOff;

    network_service->ConfigureStubHostResolver(
        /*insecure_dns_client_enabled=*/true,
        secure_dns_mode, doh_config ? *doh_config : net::DnsOverHttpsConfig(),
        /*additional_dns_types_enabled=*/true);
  }
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
