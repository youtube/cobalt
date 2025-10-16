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

#include "cobalt/shell/browser/shell_content_browser_client.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/browser/shell_browser_main_parts.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"
#include "cobalt/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "cobalt/shell/common/shell_paths.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/protocol_handler_throttle.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/performance_manager/embedder/binders.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/ssl/client_cert_identity.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "cobalt/shell/android/shell_descriptors.h"
#include "components/variations/android/variations_seed_bridge.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_descriptors.h"
#endif

#if BUILDFLAG(ENABLE_CAST_RENDERER)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if defined(RUN_BROWSER_TESTS)
#include "cobalt/shell/common/shell_controller.test-mojom.h"  // nogncheck
#include "cobalt/shell/common/shell_test_switches.h"          // nogncheck
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"  //nogncheck
#include "components/metrics/test/test_enabled_state_provider.h"  // nogncheck
#endif  // defined(RUN_BROWSER_TESTS)

namespace content {

namespace {

using PerformanceManagerRegistry =
    performance_manager::PerformanceManagerRegistry;

// Tests may install their own ShellContentBrowserClient, track the list here.
// The list is ordered with oldest first and newer ones added after it.
std::vector<ShellContentBrowserClient*>&
GetShellContentBrowserClientInstancesImpl() {
  static base::NoDestructor<std::vector<ShellContentBrowserClient*>> instances;
  return *instances;
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

#if defined(RUN_BROWSER_TESTS)
class ShellControllerImpl : public mojom::ShellController {
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
      std::move(callback).Run(std::nullopt);
    }
  }

  void ExecuteJavaScript(const std::u16string& script,
                         ExecuteJavaScriptCallback callback) override {
    CHECK(!Shell::windows().empty());
    WebContents* contents = Shell::windows()[0]->web_contents();
    contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        script, std::move(callback), ISOLATED_WORLD_ID_GLOBAL);
  }

  void ShutDown() override { Shell::Shutdown(); }
};
#endif  // defined(RUN_BROWSER_TESTS)

// TODO(crbug/1219642): Consider not needing VariationsServiceClient just to use
// VariationsFieldTrialCreator.
class ShellVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  ShellVariationsServiceClient() = default;
  ~ShellVariationsServiceClient() override = default;

  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  // Profiles aren't supported, so nothing to do here.
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}
};

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  DCHECK(frame_host);
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

base::flat_set<url::Origin> GetIsolatedContextOriginSetFromFlag() {
  std::string cmdline_origins(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIsolatedContextOrigins));

  std::vector<std::string_view> origin_strings = base::SplitStringPiece(
      cmdline_origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::flat_set<url::Origin> origin_set;
  for (const std::string_view& origin_string : origin_strings) {
    url::Origin allowed_origin = url::Origin::Create(GURL(origin_string));
    if (!allowed_origin.opaque()) {
      origin_set.insert(allowed_origin);
    } else {
      LOG(ERROR) << "Error parsing IsolatedContext origin: " << origin_string;
    }
  }
  return origin_set;
}

// In content browser tests we allow more than one ShellContentBrowserClient
// to be created (actually, ContentBrowserTestContentBrowserClient). Any state
// needed should be added here so that it's shared between the instances.
struct SharedState {
  SharedState() {}

  // Owned by content::BrowserMainLoop.
  raw_ptr<ShellBrowserMainParts, DanglingUntriaged> shell_browser_main_parts =
      nullptr;

  std::unique_ptr<PrefService> local_state;
};

SharedState& GetSharedState() {
  static SharedState* g_shared_state = nullptr;
  if (!g_shared_state) {
    g_shared_state = new SharedState();
  }
  return *g_shared_state;
}

#if defined(RUN_BROWSER_TESTS)
std::unique_ptr<PrefService> CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  base::FilePath path;
  CHECK(base::PathService::Get(SHELL_DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  return pref_service_factory.Create(pref_registry);
}
#endif  // defined(RUN_BROWSER_TESTS)

}  // namespace

std::string GetShellLanguage() {
  return "en-us,en";
}

blink::UserAgentMetadata GetShellUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list.emplace_back("content_shell",
                                           CONTENT_SHELL_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back("content_shell",
                                                CONTENT_SHELL_VERSION);
  metadata.full_version = CONTENT_SHELL_VERSION;
  metadata.platform = "Unknown";
  metadata.architecture = embedder_support::GetCpuArchitecture();
  metadata.model = embedder_support::BuildModelInfo();

  metadata.bitness = embedder_support::GetCpuBitness();
  metadata.wow64 = embedder_support::IsWoW64();

  return metadata;
}

// static
bool ShellContentBrowserClient::allow_any_cors_exempt_header_for_browser_ =
    false;

ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  auto& instances = GetShellContentBrowserClientInstancesImpl();
  return instances.empty() ? nullptr : instances.back();
}

ShellContentBrowserClient::ShellContentBrowserClient() {
  GetShellContentBrowserClientInstancesImpl().push_back(this);
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  std::erase(GetShellContentBrowserClientInstancesImpl(), this);
}

std::unique_ptr<BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts = std::make_unique<ShellBrowserMainParts>();
  DCHECK(!GetSharedState().shell_browser_main_parts);
  GetSharedState().shell_browser_main_parts = browser_main_parts.get();
  return browser_main_parts;
}

bool ShellContentBrowserClient::HasCustomSchemeHandler(
    content::BrowserContext* browser_context,
    const std::string& scheme) {
#if defined(RUN_BROWSER_TESTS)
  if (custom_handlers::ProtocolHandlerRegistry* protocol_handler_registry =
          custom_handlers::SimpleProtocolHandlerRegistryFactory::
              GetForBrowserContext(browser_context)) {
    return protocol_handler_registry->IsHandledProtocol(scheme);
  }
#endif  // defined(RUN_BROWSER_TESTS)
  return false;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ShellContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

#if defined(RUN_BROWSER_TESTS)
  auto* factory = custom_handlers::SimpleProtocolHandlerRegistryFactory::
      GetForBrowserContext(browser_context);
  // null in unit tests.
  if (factory) {
    result.push_back(
        std::make_unique<custom_handlers::ProtocolHandlerThrottle>(*factory));
  }
#endif  // defined(RUN_BROWSER_TESTS)

  return result;
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  static const char* const kProtocolList[] = {
      url::kHttpScheme, url::kHttpsScheme,        url::kWsScheme,
      url::kWssScheme,  url::kBlobScheme,         url::kFileSystemScheme,
      kChromeUIScheme,  kChromeUIUntrustedScheme, kChromeDevToolsScheme,
      url::kDataScheme, url::kFileScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (url.scheme_piece() == supported_protocol) {
      return true;
    }
  }
  return false;
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  static const char* const kForwardSwitches[] = {
      switches::kCrashDumpsDir,
      switches::kEnableCrashReporter,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardSwitches);

#if defined(RUN_BROWSER_TESTS)
  static const char* kForwardTestSwitches[] = {
      switches::kExposeInternalsForTesting,
      switches::kRunWebTests,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardTestSwitches);
#endif  // defined(RUN_BROWSER_TESTS)

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

device::GeolocationSystemPermissionManager*
ShellContentBrowserClient::GetGeolocationSystemPermissionManager() {
  return nullptr;
}

std::string ShellContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return GetShellLanguage();
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

std::unique_ptr<WebContentsViewDelegate>
ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  PerformanceManagerRegistry::GetInstance()->MaybeCreatePageNodeForWebContents(
      web_contents);
  return CreateShellWebContentsViewDelegate(web_contents);
}

bool ShellContentBrowserClient::IsIsolatedContextAllowedForUrl(
    BrowserContext* browser_context,
    const GURL& lock_url) {
  static base::flat_set<url::Origin> isolated_context_origins =
      GetIsolatedContextOriginSetFromFlag();
  return isolated_context_origins.contains(url::Origin::Create(lock_url));
}

bool ShellContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  return true;
}

bool ShellContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  return true;
}

GeneratedCodeCacheSettings
ShellContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

base::OnceClosure ShellContentBrowserClient::SelectClientCertificate(
    BrowserContext* browser_context,
    int process_id,
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {
  if (select_client_certificate_callback_ && web_contents) {
    return std::move(select_client_certificate_callback_)
        .Run(web_contents, cert_request_info, std::move(client_certs),
             std::move(delegate));
  }
  return base::OnceClosure();
}

SpeechRecognitionManagerDelegate*
ShellContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new ShellSpeechRecognitionManagerDelegate();
}

void ShellContentBrowserClient::OverrideWebPreferences(
    WebContents* web_contents,
    SiteInstance& main_frame_site,
    blink::web_pref::WebPreferences* prefs) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast)) {
    prefs->in_forced_colors = true;
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kMore;
  } else {
    prefs->in_forced_colors = false;
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
  }

  if (override_web_preferences_callback_) {
    override_web_preferences_callback_.Run(prefs);
  }
}

std::unique_ptr<content::DevToolsManagerDelegate>
ShellContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<ShellDevToolsManagerDelegate>(browser_context());
}

void ShellContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  PerformanceManagerRegistry::GetInstance()->CreateProcessNode(
      render_process_host);
  PerformanceManagerRegistry::GetInstance()
      ->GetBinders()
      .ExposeInterfacesToRendererProcess(registry, render_process_host);
}

mojo::Remote<::media::mojom::MediaService>
ShellContentBrowserClient::RunSecondaryMediaService() {
  mojo::Remote<::media::mojom::MediaService> remote;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  static base::SequenceLocalStorageSlot<std::unique_ptr<::media::MediaService>>
      service;
  service.emplace(::media::CreateMediaServiceForTesting(
      remote.BindNewPipeAndPassReceiver()));
#endif
  return remote;
}

void ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  PerformanceManagerRegistry::GetInstance()
      ->GetBinders()
      .ExposeInterfacesToRenderFrame(map);
  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
}

void ShellContentBrowserClient::OpenURL(
    SiteInstance* site_instance,
    const OpenURLParams& params,
    base::OnceCallback<void(WebContents*)> callback) {
  std::move(callback).Run(
      Shell::CreateNewWindow(site_instance->GetBrowserContext(), params.url,
                             nullptr, gfx::Size())
          ->web_contents());
}

void ShellContentBrowserClient::CreateThrottlesForNavigation(
    NavigationThrottleRegistry& registry) {
  if (create_throttles_for_navigation_callback_) {
    create_throttles_for_navigation_callback_.Run(registry);
  }
}

std::unique_ptr<LoginDelegate> ShellContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame_navigation,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    GuestPageHolder* guest,
    LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  if (!login_request_callback_.is_null()) {
    std::move(login_request_callback_)
        .Run(is_request_for_primary_main_frame_navigation,
             is_request_for_navigation);
  }
  return nullptr;
}

base::Value::Dict ShellContentBrowserClient::GetNetLogConstants() {
  base::Value::Dict client_constants;
  client_constants.Set("name", "content_shell");
  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
  client_constants.Set("command_line", command_line);
  base::Value::Dict constants;
  constants.Set("clientInfo", std::move(client_constants));
  return constants;
}

base::FilePath
ShellContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return browser_context()->GetPath();
}

base::FilePath ShellContentBrowserClient::GetFirstPartySetsDirectory() {
  return browser_context()->GetPath();
}

std::string ShellContentBrowserClient::GetUserAgent() {
  const auto custom_ua = embedder_support::GetUserAgentFromCommandLine();
  if (custom_ua.has_value()) {
    return custom_ua.value();
  }

  std::string product =
      base::StringPrintf("Chrome/%s.0.0.0", CONTENT_SHELL_MAJOR_VERSION);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          embedder_support::kUseMobileUserAgent)) {
    product += " Mobile";
  }
#endif

  return embedder_support::BuildUnifiedPlatformUserAgentFromProduct(product);
}

blink::UserAgentMetadata ShellContentBrowserClient::GetUserAgentMetadata() {
  return GetShellUserAgentMetadata();
}

void ShellContentBrowserClient::OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    bool is_for_service_worker,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (url_loader_factory_params_callback_) {
    url_loader_factory_params_callback_.Run(
        factory_params, origin, is_for_isolated_world, is_for_service_worker);
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
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

void ShellContentBrowserClient::ConfigureNetworkContextParams(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ConfigureNetworkContextParamsForShell(context, network_context_params,
                                        cert_verifier_creation_params);
}

std::vector<base::FilePath>
ShellContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {browser_context()->GetPath()};
}

#if BUILDFLAG(IS_IOS)
BluetoothDelegate* ShellContentBrowserClient::GetBluetoothDelegate() {
  if (!bluetooth_delegate_) {
    bluetooth_delegate_ = std::make_unique<permissions::BluetoothDelegateImpl>(
        std::make_unique<ShellBluetoothDelegateImplClient>());
  }
  return bluetooth_delegate_.get();
}
#endif

#if defined(RUN_BROWSER_TESTS)
void ShellContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {
  if (!pipe.is_valid()) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ShellControllerImpl>(),
      mojo::PendingReceiver<mojom::ShellController>(std::move(pipe)));
}
#endif  // defined(RUN_BROWSER_TESTS)

void ShellContentBrowserClient::set_browser_main_parts(
    ShellBrowserMainParts* parts) {
  GetSharedState().shell_browser_main_parts = parts;
}

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return GetSharedState().shell_browser_main_parts->browser_context();
}

ShellBrowserContext*
ShellContentBrowserClient::off_the_record_browser_context() {
  return GetSharedState()
      .shell_browser_main_parts->off_the_record_browser_context();
}

ShellBrowserMainParts* ShellContentBrowserClient::shell_browser_main_parts() {
  return GetSharedState().shell_browser_main_parts;
}

void ShellContentBrowserClient::ConfigureNetworkContextParamsForShell(
    BrowserContext* context,
    network::mojom::NetworkContextParams* context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  context_params->allow_any_cors_exempt_header_for_browser =
      allow_any_cors_exempt_header_for_browser_;
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = GetAcceptLangs(context);
  auto exempt_header =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "cors_exempt_header_list");
  if (!exempt_header.empty()) {
    context_params->cors_exempt_header_list.push_back(exempt_header);
  }
}

void ShellContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
#if BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
  // TODO: Investigate if we can remove this function or need to implement it.
  NOTIMPLEMENTED();
#else
  // If we have the source tree, return the dictionary files in the tree.
  base::FilePath dir;
  if (base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir)) {
    dir = dir.AppendASCII("third_party")
              .AppendASCII("hyphenation-patterns")
              .AppendASCII("hyb");
    std::move(callback).Run(dir);
  }
  // No need to callback if there were no dictionaries.
#endif
}

bool ShellContentBrowserClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400 && http_status_code < 600;
}

void ShellContentBrowserClient::CreateFeatureListAndFieldTrials() {
#if defined(RUN_BROWSER_TESTS)
  GetSharedState().local_state = CreateLocalState();
  SetUpFieldTrials();
  // Schedule a Local State write since the above function resulted in some
  // prefs being updated.
  GetSharedState().local_state->CommitPendingWrite();
#endif  // defined(RUN_BROWSER_TESTS)
}

void ShellContentBrowserClient::SetUpFieldTrials() {
#if defined(RUN_BROWSER_TESTS)
  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                           /*enabled=*/false);
  base::FilePath path;
  base::PathService::Get(SHELL_DIR_USER_DATA, &path);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          GetSharedState().local_state.get(), &enabled_state_provider,
          std::wstring(), path, metrics::StartupVisibility::kUnknown,
          {
              .force_benchmarking_mode =
                  base::CommandLine::ForCurrentProcess()->HasSwitch(
                      switches::kEnableGpuBenchmarking),
          });
  metrics_state_manager->InstantiateFieldTrialList();

  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();

  std::unique_ptr<variations::SeedResponse> initial_seed;
#if BUILDFLAG(IS_ANDROID)
  if (!GetSharedState().local_state->HasPrefPath(
          variations::prefs::kVariationsSeedSignature)) {
    DVLOG(1) << "Importing first run seed from Java preferences.";
    initial_seed = variations::android::GetVariationsFirstRunSeed();
  }
#endif

  ShellVariationsServiceClient variations_service_client;
  variations::VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client,
      std::make_unique<variations::VariationsSeedStore>(
          GetSharedState().local_state.get(), std::move(initial_seed),
          /*signature_verification_enabled=*/true,
          std::make_unique<variations::VariationsSafeSeedStoreLocalState>(
              GetSharedState().local_state.get(),
              variations_service_client.GetVariationsSeedFileDir(),
              variations_service_client.GetChannelForVariations(),
              /*entropy_providers=*/nullptr),
          variations_service_client.GetChannelForVariations(),
          variations_service_client.GetVariationsSeedFileDir()),
      variations::UIStringOverrider(),
      // The limited entropy synthetic trial will not be registered for this
      // purpose.
      /*limited_entropy_synthetic_trial=*/nullptr);

  variations::SafeSeedManager safe_seed_manager(
      GetSharedState().local_state.get());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Overrides for content/common and lower layers' switches.
  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      content::GetSwitchDependentFeatureOverrides(command_line);

  // Overrides for content/shell switches.

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

  // Since this is a test-only code path, some arguments to SetUpFieldTrials are
  // null.
  // TODO(crbug/1248066): Consider passing a low entropy source.
  variations::PlatformFieldTrials platform_field_trials;
  variations::SyntheticTrialRegistry synthetic_trial_registry;
  field_trial_creator.SetUpFieldTrials(
      variation_ids,
      command_line.GetSwitchValueASCII(
          variations::switches::kForceVariationIds),
      feature_overrides, std::move(feature_list), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/false,
      *metrics_state_manager->CreateEntropyProviders(
          /*enable_limited_entropy_mode=*/false));
#endif  // defined(RUN_BROWSER_TESTS)
}

std::optional<network::ParsedPermissionsPolicy>
ShellContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    content::WebContents* web_contents,
    const url::Origin& app_origin) {
  network::ParsedPermissionsPolicyDeclaration coi_decl(
      network::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
      /*allowed_origins=*/{},
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/true, /*matches_opaque_src=*/false);

  network::ParsedPermissionsPolicyDeclaration socket_decl(
      network::mojom::PermissionsPolicyFeature::kDirectSockets,
      /*allowed_origins=*/{}, app_origin,
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);
  return {{coi_decl, socket_decl}};
}

// Tests may install their own ShellContentBrowserClient, track the list here.
// The list is ordered with oldest first and newer ones added after it.
// static
const std::vector<ShellContentBrowserClient*>&
ShellContentBrowserClient::GetShellContentBrowserClientInstances() {
  return GetShellContentBrowserClientInstancesImpl();
}

}  // namespace content
