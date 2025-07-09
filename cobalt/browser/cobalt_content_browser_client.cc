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

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/buildflag.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/cobalt_secure_navigation_throttle.h"
#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/user_agent/user_agent_platform_info.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/browser/shell_browser_main_parts.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "cobalt/shell/browser/shell_speech_recognition_manager_delegate.h"
#include "cobalt/shell/browser/shell_web_contents_view_delegate.h"
#include "cobalt/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/protocol_handler_throttle.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/ukm/scheme_constants.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/posix_file_descriptor_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/common/shell_controller.test-mojom.h"
#include "content/shell/common/shell_switches.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_descriptors.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

#if defined(ENABLE_CAST_RENDERER)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

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

class ShellControllerImpl : public content::mojom::ShellController {
 public:
  ShellControllerImpl() = default;
  ~ShellControllerImpl() override = default;

  // mojom::ShellController:
  void GetSwitchValue(const std::string& name,
                      GetSwitchValueCallback callback) override {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(name)) {
      std::move(callback).Run(command_line.GetSwitchValueASCII(name));
    } else {
      std::move(callback).Run(absl::nullopt);
    }
  }

  void ExecuteJavaScript(const std::u16string& script,
                         ExecuteJavaScriptCallback callback) override {
    CHECK(!content::Shell::windows().empty());
    content::WebContents* contents =
        content::Shell::windows()[0]->web_contents();
    contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        script, std::move(callback));
  }

  void ShutDown() override { content::Shell::Shutdown(); }
};

std::unique_ptr<PrefService> CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  base::FilePath path;
  CHECK(base::PathService::Get(content::SHELL_DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  return pref_service_factory.Create(pref_registry);
}

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  DCHECK(frame_host);
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

#if BUILDFLAG(IS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif BUILDFLAG(IS_LINUX)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  int fd;
  pid_t pid;
  return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
}
#endif

#if BUILDFLAG(IS_ANDROID)
// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
  kShellPakDescriptor = kContentIPCDescriptorMax + 1,
  kAndroidMinidumpDescriptor,
};
#endif  // BUILDFLAG(IS_ANDROID)

std::string GetShellLanguage() {
  return "en-us,en";
}

base::flat_set<url::Origin> GetIsolatedContextOriginSetFromFlag() {
  std::string cmdline_origins(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIsolatedContextOrigins));

  std::vector<base::StringPiece> origin_strings = base::SplitStringPiece(
      cmdline_origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::flat_set<url::Origin> origin_set;
  for (const base::StringPiece& origin_string : origin_strings) {
    url::Origin allowed_origin = url::Origin::Create(GURL(origin_string));
    if (!allowed_origin.opaque()) {
      origin_set.insert(allowed_origin);
    } else {
      LOG(ERROR) << "Error parsing IsolatedContext origin: " << origin_string;
    }
  }
  return origin_set;
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

content::BrowserContext* CobaltContentBrowserClient::GetBrowserContext() {
  return shell_browser_main_parts_->browser_context();
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
  DCHECK(!shell_browser_main_parts_);

  auto browser_main_parts = std::make_unique<CobaltBrowserMainParts>();
  shell_browser_main_parts_ = browser_main_parts.get();

  return browser_main_parts;
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
CobaltContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  throttles.push_back(
      std::make_unique<content::CobaltSecureNavigationThrottle>(handle));
  return throttles;
}

content::GeneratedCodeCacheSettings
CobaltContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // Default compiled javascript quota in Cobalt 25.
  // https://github.com/youtube/cobalt/blob/3ccdb04a5e36c2597fe7066039037eabf4906ba5/cobalt/network/disk_cache/resource_type.cc#L72
  constexpr size_t size = 3 * 1024 * 1024;
  return content::GeneratedCodeCacheSettings(/*enabled=*/true, size,
                                             context->GetPath());
}

std::string CobaltContentBrowserClient::GetApplicationLocale() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  return base::android::GetDefaultLocaleString();
#else
  return base::i18n::GetConfiguredLocale();
#endif
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast)) {
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kMore;
  } else {
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
  }
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
  base::FilePath base_cache_path = context->GetPath();
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
  web_contents_delegate_.reset(new CobaltWebContentsDelegate());
  content::Shell::SetShellCreatedCallback(base::BindOnce(
      [](content::WebContentsDelegate* delegate, content::Shell* shell) {
        shell->web_contents()->SetDelegate(delegate);
      },
      web_contents_delegate_.get()));
}

void CobaltContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PopulateCobaltFrameBinders(render_frame_host, map);
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->ExposeInterfacesToRenderFrame(map);
  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
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
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->CreateProcessNodeAndExposeInterfacesToRendererProcess(
          registry, render_process_host);
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
void CobaltContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if BUILDFLAG(IS_ANDROID)
  mappings->ShareWithRegion(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor));
#endif
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID)

bool CobaltContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  static const char* const kProtocolList[] = {
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kWsScheme,
      url::kWssScheme,
      url::kBlobScheme,
      url::kFileSystemScheme,
      ukm::kChromeUIScheme,
      content::kChromeUIUntrustedScheme,
      content::kChromeDevToolsScheme,
      url::kDataScheme,
      url::kFileScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (url.scheme_piece() == supported_protocol) {
      return true;
    }
  }
  return false;
}

bool CobaltContentBrowserClient::HasCustomSchemeHandler(
    content::BrowserContext* browser_context,
    const std::string& scheme) {
  if (custom_handlers::ProtocolHandlerRegistry* protocol_handler_registry =
          custom_handlers::SimpleProtocolHandlerRegistryFactory::
              GetForBrowserContext(browser_context)) {
    return protocol_handler_registry->IsHandledProtocol(scheme);
  }
  return false;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CobaltContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  auto* factory = custom_handlers::SimpleProtocolHandlerRegistryFactory::
      GetForBrowserContext(browser_context);
  // null in unit tests.
  if (factory) {
    result.push_back(
        std::make_unique<custom_handlers::ProtocolHandlerThrottle>(*factory));
  }

  return result;
}

void CobaltContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  static const char* kForwardSwitches[] = {
      switches::kCrashDumpsDir,
      switches::kEnableCrashReporter,
      switches::kExposeInternalsForTesting,
      switches::kRunWebTests,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardSwitches, std::size(kForwardSwitches));

#if BUILDFLAG(IS_LINUX)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  }
#endif  // BUILDFLAG(IS_LINUX)
}

std::string CobaltContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return GetShellLanguage();
}

std::string CobaltContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

std::unique_ptr<content::WebContentsViewDelegate>
CobaltContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->MaybeCreatePageNodeForWebContents(web_contents);
  return CreateShellWebContentsViewDelegate(web_contents);
}

bool CobaltContentBrowserClient::IsIsolatedContextAllowedForUrl(
    content::BrowserContext* browser_context,
    const GURL& lock_url) {
  static base::flat_set<url::Origin> isolated_context_origins =
      GetIsolatedContextOriginSetFromFlag();
  return isolated_context_origins.contains(url::Origin::Create(lock_url));
}

bool CobaltContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  return true;
}

bool CobaltContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  return true;
}

content::SpeechRecognitionManagerDelegate*
CobaltContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new content::ShellSpeechRecognitionManagerDelegate();
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

std::unique_ptr<content::DevToolsManagerDelegate>
CobaltContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<content::ShellDevToolsManagerDelegate>(
      GetBrowserContext());
}

void CobaltContentBrowserClient::SetUpCobaltFeaturesAndParams(
    base::FeatureList* feature_list) {
  // All Cobalt features are associated with the same field trial. This is for
  // easier feature param lookup.
  base::FieldTrial* cobalt_field_trial = base::FieldTrialList::CreateFieldTrial(
      kCobaltExperimentName, kCobaltGroupName);
  CHECK(cobalt_field_trial) << "Unexpected name conflict.";

  auto experiment_config = GlobalFeatures::GetInstance()->experiment_config();

  const base::Value::Dict& feature_map =
      experiment_config->GetDict(kExperimentConfigFeatures);
  const base::Value::Dict& param_map =
      experiment_config->GetDict(kExperimentConfigFeatureParams);

  for (const auto feature_name_and_value : feature_map) {
    if (feature_name_and_value.second.is_bool()) {
      auto override_value =
          feature_name_and_value.second.GetBool()
              ? base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE
              : base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE;
      feature_list->RegisterFieldTrialOverride(
          feature_name_and_value.first, override_value, cobalt_field_trial);
    } else {
      // TODO(b/407734134): Register UMA here for non boolean feature value.
      LOG(ERROR) << "Failed to apply override for feature "
                 << feature_name_and_value.first;
      base::debug::DumpWithoutCrashing();
    }
  }

  base::FieldTrialParams params;
  for (const auto param_name_and_value : param_map) {
    if (param_name_and_value.second.is_string()) {
      params.emplace(param_name_and_value.first,
                     param_name_and_value.second.GetString());
    } else {
      // TODO(b/407734134): Register UMA here for non string param value.
      LOG(ERROR) << "Failed to associate field trial param "
                 << param_name_and_value.first << " with string value "
                 << param_name_and_value.second;
      base::debug::DumpWithoutCrashing();
    }
  }
  base::AssociateFieldTrialParams(kCobaltExperimentName, kCobaltGroupName,
                                  params);
}

void CobaltContentBrowserClient::CreateFeatureListAndFieldTrials() {
  local_state_ = CreateLocalState();
  GlobalFeatures::GetInstance()
      ->metrics_services_manager()
      ->InstantiateFieldTrialList();
  // Schedule a Local State write since the above function resulted in some
  // prefs being updated.
  local_state_->CommitPendingWrite();

  auto feature_list = std::make_unique<base::FeatureList>();

  auto accessor = feature_list->ConstructAccessor();
  GlobalFeatures::GetInstance()->set_accessor(std::move(accessor));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Overrides for content/common and lower layers' switches.
  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      content::GetSwitchDependentFeatureOverrides(command_line);

  // Overrides for --run-web-tests.
  if (switches::IsRunWebTestsSwitchPresent()) {
    // Disable artificial timeouts for PNA-only preflights in warning-only mode
    // for web tests. We do not exercise this behavior with web tests as it is
    // intended to be a temporary rollout stage, and the short timeout causes
    // flakiness when the test server takes just a tad too long to respond.
    feature_overrides.emplace_back(
        std::cref(
            network::features::kPrivateNetworkAccessPreflightShortTimeout),
        base::FeatureList::OVERRIDE_DISABLE_FEATURE);
  }

  feature_list->InitializeFromCommandLine(
      command_line.GetSwitchValueASCII(::switches::kEnableFeatures),
      command_line.GetSwitchValueASCII(::switches::kDisableFeatures));

  // This needs to happen here: After the InitFromCommandLine() call,
  // because the explicit cmdline --disable-features and --enable-features
  // should take precedence over these extra overrides. Before the call to
  // SetInstance(), because overrides cannot be registered after the FeatureList
  // instance is set.
  feature_list->RegisterExtraFeatureOverrides(feature_overrides);

  SetUpCobaltFeaturesAndParams(feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));
  LOG(INFO) << "CobaltCommandLine "
            << command_line.GetSwitchValueASCII(::switches::kEnableFeatures);
  LOG(INFO) << "CobaltCommandLine "
            << command_line.GetSwitchValueASCII(::switches::kDisableFeatures);
}

void CobaltContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  std::move(callback).Run(
      content::Shell::CreateNewWindow(site_instance->GetBrowserContext(),
                                      params.url, nullptr, gfx::Size())
          ->web_contents());
}

base::Value::Dict CobaltContentBrowserClient::GetNetLogConstants() {
  base::Value::Dict client_constants;
  client_constants.Set("name", "content_shell");
  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
  client_constants.Set("command_line", command_line);
  base::Value::Dict constants;
  constants.Set("clientInfo", std::move(client_constants));
  return constants;
}

std::vector<base::FilePath>
CobaltContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {GetBrowserContext()->GetPath()};
}

void CobaltContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {
  if (!pipe.is_valid()) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ShellControllerImpl>(),
      mojo::PendingReceiver<content::mojom::ShellController>(std::move(pipe)));
}

void CobaltContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
  // If we have the source tree, return the dictionary files in the tree.
  base::FilePath dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &dir)) {
    dir = dir.AppendASCII("third_party")
              .AppendASCII("hyphenation-patterns")
              .AppendASCII("hyb");
    std::move(callback).Run(dir);
  }
  // No need to callback if there were no dictionaries.
}

bool CobaltContentBrowserClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400 && http_status_code < 600;
}

absl::optional<blink::ParsedPermissionsPolicy>
CobaltContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    content::BrowserContext* browser_context,
    const url::Origin& app_origin) {
  blink::ParsedPermissionsPolicyDeclaration decl(
      blink::mojom::PermissionsPolicyFeature::kDirectSockets,
      {blink::OriginWithPossibleWildcards(app_origin,
                                          /*has_subdomain_wildcard=*/false)},
      /*self_if_matches=*/absl::nullopt,
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);
  return {{decl}};
}

}  // namespace cobalt
