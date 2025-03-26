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

#include "cobalt/browser/cobalt_content_browser_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "cobalt/user_agent/user_agent_platform_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace cobalt {

namespace {

constexpr base::FilePath::CharType kCacheDirname[] = FILE_PATH_LITERAL("Cache");
constexpr base::FilePath::CharType kCookieFilename[] =
    FILE_PATH_LITERAL("Cookies");
constexpr base::FilePath::CharType kNetworkDataDirname[] =
    FILE_PATH_LITERAL("Network");
constexpr base::FilePath::CharType kNetworkPersistentStateFilename[] =
    FILE_PATH_LITERAL("Network Persistent State");
constexpr base::FilePath::CharType kSCTAuditingPendingReportsFileName[] =
    FILE_PATH_LITERAL("SCT Auditing Pending Reports");
constexpr base::FilePath::CharType kTransportSecurityPersisterFilename[] =
    FILE_PATH_LITERAL("TransportSecurity");
constexpr base::FilePath::CharType kTrustTokenFilename[] =
    FILE_PATH_LITERAL("Trust Tokens");

base::FilePath GetCacheDirectory() {
  std::string path;
  path.resize(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    NOTREACHED() << "Failed to get cache directory.";
    return base::FilePath();
  }
  return base::FilePath(path.data());
}

}  // namespace

std::string GetCobaltUserAgent() {
// TODO: (cobalt b/375243230) enable UserAgentPlatformInfo on Linux.
#if BUILDFLAG(IS_ANDROID)
  const UserAgentPlatformInfo platform_info;
  static const std::string user_agent_str = platform_info.ToString();
  return user_agent_str;
#else
  return std::string(
      "Mozilla/5.0 (X11; Linux x86_64) Cobalt/26.lts.0-qa (unlike Gecko) "
      "v8/unknown gles Starboard/17, "
      "SystemIntegratorName_DESKTOP_ChipsetModelNumber_2025/FirmwareVersion "
      "(BrandName, ModelName)");
#endif
}

blink::UserAgentMetadata GetCobaltUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

#define COBALT_BRAND_NAME "Cobalt"
#define COBALT_MAJOR_VERSION "26"
#define COBALT_VERSION "26.lts.0-qa"
  metadata.brand_version_list.emplace_back(COBALT_BRAND_NAME,
                                           COBALT_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back(COBALT_BRAND_NAME,
                                                COBALT_VERSION);
  metadata.full_version = COBALT_VERSION;
  metadata.platform = "Starboard";
  metadata.architecture = content::GetCpuArchitecture();
  metadata.model = content::BuildModelInfo();

  metadata.bitness = content::GetCpuBitness();
  metadata.wow64 = content::IsWoW64();

  return metadata;
}

CobaltContentBrowserClient::CobaltContentBrowserClient()
    : video_geometry_setter_service_(
          std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                          base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(nullptr))) {
  DETACH_FROM_THREAD(thread_checker_);
}

CobaltContentBrowserClient::~CobaltContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
CobaltContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto browser_main_parts =
      std::make_unique<CobaltBrowserMainParts>(GetCacheDirectory());
  set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts;
}

std::string CobaltContentBrowserClient::GetApplicationLocale() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::i18n::GetConfiguredLocale();
}

std::string CobaltContentBrowserClient::GetUserAgent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCobaltUserAgent();
}

std::string CobaltContentBrowserClient::GetFullUserAgent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCobaltUserAgent();
}

std::string CobaltContentBrowserClient::GetReducedUserAgent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCobaltUserAgent();
}

blink::UserAgentMetadata CobaltContentBrowserClient::GetUserAgentMetadata() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCobaltUserAgentMetadata();
}

void CobaltContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if !defined(COBALT_IS_RELEASE_BUILD)
  // Allow creating a ws: connection on a https: page to allow current
  // testing set up. See b/377410179.
  prefs->allow_running_insecure_content = true;
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  content::ShellContentBrowserClient::OverrideWebkitPrefs(web_contents, prefs);
}

content::StoragePartitionConfig
CobaltContentBrowserClient::GetStoragePartitionConfigForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  // Default to the browser-wide storage partition and override based on |site|
  // below.
  content::StoragePartitionConfig default_storage_partition_config =
      content::StoragePartitionConfig::CreateDefault(browser_context);

  return default_storage_partition_config;
}

void CobaltContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  base::FilePath base_cache_path = GetCacheDirectory();
  base::FilePath path = base_cache_path.Append(relative_partition_path);
  network_context_params->user_agent = GetCobaltUserAgent();
  network_context_params->enable_referrers = true;
  network_context_params->quic_user_agent_id = "";
  network_context_params->accept_language = GetApplicationLocale();

  // Always enable the HTTP cache.
  network_context_params->http_cache_enabled = true;

  auto cookie_manager_params = network::mojom::CookieManagerParams::New();
  cookie_manager_params->block_third_party_cookies = true;
  network_context_params->cookie_manager_params =
      std::move(cookie_manager_params);

  // Configure on-disk storage for non-off-the-record profiles. Off-the-record
  // profiles just use default behavior (in memory storage, default sizes).
  if (!in_memory) {
    network_context_params->http_cache_directory =
        base_cache_path.Append(kCacheDirname);

    network_context_params->file_paths =
        ::network::mojom::NetworkContextFilePaths::New();

    network_context_params->file_paths->data_directory =
        path.Append(kNetworkDataDirname);
    network_context_params->file_paths->unsandboxed_data_path = path;

    // Currently this just contains HttpServerProperties, but that will likely
    // change.
    network_context_params->file_paths->http_server_properties_file_name =
        base::FilePath(kNetworkPersistentStateFilename);
    network_context_params->file_paths->cookie_database_name =
        base::FilePath(kCookieFilename);
    network_context_params->file_paths->trust_token_database_name =
        base::FilePath(kTrustTokenFilename);

    network_context_params->restore_old_session_cookies = false;
    network_context_params->persist_session_cookies = true;

    network_context_params->file_paths->transport_security_persister_file_name =
        base::FilePath(kTransportSecurityPersisterFilename);
    network_context_params->file_paths->sct_auditing_pending_reports_file_name =
        base::FilePath(kSCTAuditingPendingReportsFileName);
  }

  network_context_params->enable_certificate_reporting = true;

  network_context_params->sct_auditing_mode =
      network::mojom::SCTAuditingMode::kDisabled;

  // All consumers of the main NetworkContext must provide NetworkIsolationKeys
  // / IsolationInfos, so storage can be isolated on a per-site basis.
  network_context_params->require_network_isolation_key = true;
}

void CobaltContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  web_contents_observer_.reset(new CobaltWebContentsObserver(web_contents));
}

void CobaltContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PopulateCobaltFrameBinders(render_frame_host, map);
  ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
}

void CobaltContentBrowserClient::CreateVideoGeometrySetterService() {
  DCHECK(!video_geometry_setter_service_);
  video_geometry_setter_service_ =
      std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                      base::OnTaskRunnerDeleter>(
          new media::VideoGeometrySetterService,
          base::OnTaskRunnerDeleter(
              base::SingleThreadTaskRunner::GetCurrentDefault()));
}

void CobaltContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  registry->AddInterface<cobalt::media::mojom::VideoGeometryChangeSubscriber>(
      video_geometry_setter_service_->GetBindSubscriberCallback(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void CobaltContentBrowserClient::BindGpuHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  if (auto r = receiver.As<media::mojom::VideoGeometrySetter>()) {
    video_geometry_setter_service_->GetVideoGeometrySetter(std::move(r));
  }
}

bool CobaltContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  if (header_client) {
    auto receiver = header_client->InitWithNewPipeAndPassReceiver();
    auto cobalt_header_client =
        std::make_unique<browser::CobaltTrustedURLLoaderHeaderClient>(
            std::move(receiver));
    cobalt_header_clients_.push_back(std::move(cobalt_header_client));
  }

  return true;
}

}  // namespace cobalt
