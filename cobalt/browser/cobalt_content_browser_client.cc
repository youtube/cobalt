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
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/cobalt_secure_navigation_throttle.h"
#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/browser/user_agent/user_agent_platform_info.h"
#include "cobalt/common/features/starboard_features_initialization.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/common/shell_paths.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/version.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(RUN_BROWSER_TESTS)
#include "cobalt/shell/common/shell_test_switches.h"  // nogncheck
#endif  // defined(RUN_BROWSER_TESTS)

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

}  // namespace

std::string GetCobaltUserAgent() {
  const UserAgentPlatformInfo platform_info;
  static const std::string user_agent_str = platform_info.ToString();
  DCHECK(!user_agent_str.empty());
  return user_agent_str;
}

blink::UserAgentMetadata GetCobaltUserAgentMetadata() {
  blink::UserAgentMetadata metadata;
  const UserAgentPlatformInfo platform_info;
  metadata.brand_version_list.emplace_back(platform_info.brand().value_or(""),
                                           COBALT_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back(
      platform_info.brand().value_or(""), platform_info.cobalt_version());
  metadata.full_version = platform_info.cobalt_version();
  metadata.platform = "Starboard";
  metadata.architecture = embedder_support::GetCpuArchitecture();
  metadata.model = embedder_support::BuildModelInfo();

  metadata.bitness = embedder_support::GetCpuBitness();
  metadata.wow64 = embedder_support::IsWoW64();

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
  auto browser_main_parts = std::make_unique<CobaltBrowserMainParts>();
  set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts;
}

void CobaltContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& navigation_handle = registry.GetNavigationHandle();
  registry.AddThrottle(
      std::make_unique<content::CobaltSecureNavigationThrottle>(
          &navigation_handle));
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
#if !defined(OFFICIAL_BUILD)
  const auto custom_ua = embedder_support::GetUserAgentFromCommandLine();
  if (custom_ua.has_value()) {
    return custom_ua.value();
  }
#endif  // !defined(OFFICIAL_BUILD)
  return GetCobaltUserAgent();
}

blink::UserAgentMetadata CobaltContentBrowserClient::GetUserAgentMetadata() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCobaltUserAgentMetadata();
}

void CobaltContentBrowserClient::OverrideWebPreferences(
    content::WebContents* web_contents,
    content::SiteInstance& main_frame_site,
    blink::web_pref::WebPreferences* prefs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if !defined(COBALT_IS_RELEASE_BUILD)
  // Allow creating a ws: connection on a https: page to allow current
  // testing set up. See b/377410179.
  prefs->allow_running_insecure_content = true;
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  content::ShellContentBrowserClient::OverrideWebPreferences(
      web_contents, main_frame_site, prefs);
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
    network_context_params->file_paths =
        ::network::mojom::NetworkContextFilePaths::New();

    network_context_params->file_paths->http_cache_directory =
        base_cache_path.Append(kCacheDirname);

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

    // Always try to restore old session cookies.
    network_context_params->restore_old_session_cookies = true;
    network_context_params->persist_session_cookies = true;

    network_context_params->file_paths->transport_security_persister_file_name =
        base::FilePath(kTransportSecurityPersisterFilename);
    network_context_params->file_paths->sct_auditing_pending_reports_file_name =
        base::FilePath(kSCTAuditingPendingReportsFileName);
  }

  network_context_params->enable_certificate_reporting = true;

  network_context_params->sct_auditing_mode =
      network::mojom::SCTAuditingMode::kDisabled;

  // All consumers of the main NetworkContext must provide
  // NetworkAnonymizationKey / IsolationInfos, so storage can be isolated on a
  // per-site basis.
  network_context_params->require_network_anonymization_key = true;
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

void CobaltContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  if (header_client) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<browser::CobaltTrustedURLLoaderHeaderClient>(),
        header_client->InitWithNewPipeAndPassReceiver());
  }
}

void CobaltContentBrowserClient::SetUpCobaltFeaturesAndParams(
    base::FeatureList* feature_list) {
  // All Cobalt features are associated with the same field trial. This is for
  // easier feature param lookup.
  base::FieldTrial* cobalt_field_trial = base::FieldTrialList::CreateFieldTrial(
      kCobaltExperimentName, kCobaltGroupName);
  CHECK(cobalt_field_trial) << "Unexpected name conflict.";

  auto* global_features = GlobalFeatures::GetInstance();
  auto* experiment_config_manager =
      global_features->experiment_config_manager();
  auto config_type = experiment_config_manager->GetExperimentConfigType();
  if (config_type == ExperimentConfigType::kEmptyConfig) {
    return;
  }
  auto* experiment_config = global_features->experiment_config();
  const bool use_safe_config =
      (config_type == ExperimentConfigType::kSafeConfig);

  const base::Value::Dict& feature_map = experiment_config->GetDict(
      use_safe_config ? kSafeConfigFeatures : kExperimentConfigFeatures);
  const base::Value::Dict& param_map = experiment_config->GetDict(
      use_safe_config ? kSafeConfigFeatureParams
                      : kExperimentConfigFeatureParams);

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
  auto* global_features = GlobalFeatures::GetInstance();
  global_features->metrics_services_manager()->InstantiateFieldTrialList();
  // Mark the session as unclean at startup. If the session exits cleanly, it
  // will be marked as clean in CobaltMetricsServiceClient's destructor.
  global_features->metrics_services_manager_client()
      ->GetMetricsStateManager()
      ->LogHasSessionShutdownCleanly(false, false);

  auto feature_list = std::make_unique<base::FeatureList>();

  auto accessor = feature_list->ConstructAccessor();
  GlobalFeatures::GetInstance()->set_accessor(std::move(accessor));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Overrides for content/common and lower layers' switches.
  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      content::GetSwitchDependentFeatureOverrides(command_line);

#if defined(RUN_BROWSER_TESTS)
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
#endif  // defined(RUN_BROWSER_TESTS)

  feature_list->InitFromCommandLine(
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
  LOG(INFO) << "CobaltCommandLine: enable_features=["
            << command_line.GetSwitchValueASCII(::switches::kEnableFeatures)
            << "], disable_features=["
            << command_line.GetSwitchValueASCII(::switches::kDisableFeatures)
            << "]";

  // Push the initialized features and params down to Starboard.
  features::InitializeStarboardFeatures();
}

}  // namespace cobalt
