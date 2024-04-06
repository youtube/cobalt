// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/url_request_context.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/network/disk_cache/cobalt_backend_factory.h"
#include "cobalt/network/disk_cache/cobalt_backend_impl.h"
#include "cobalt/network/disk_cache/resource_type.h"
#include "cobalt/network/job_factory_config.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/persistent_cookie_store.h"
#include "cobalt/network/proxy_config_service.h"
#include "cobalt/network/switches.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "starboard/common/murmurhash2.h"
#include "starboard/configuration_constants.h"

namespace cobalt {
namespace network {
namespace {

const char kPersistentSettingsJson[] = "cache_settings.json";

void LoadDiskCacheQuotaSettings(
    cobalt::persistent_storage::PersistentSettings* settings,
    int64_t max_bytes) {
  auto total_size = 0;
  std::map<disk_cache::ResourceType, uint32_t> quotas;
  for (int i = 0; i < disk_cache::kTypeCount; i++) {
    disk_cache::ResourceType resource_type = (disk_cache::ResourceType)i;
    std::string directory =
        disk_cache::defaults::GetSubdirectory(resource_type);
    uint32_t bucket_size =
        static_cast<uint32_t>(settings->GetPersistentSettingAsDouble(
            directory, disk_cache::defaults::GetQuota(resource_type)));
    quotas[resource_type] = bucket_size;
    total_size += bucket_size;
  }

  if (total_size <= max_bytes) {
    for (int i = 0; i < disk_cache::kTypeCount; i++) {
      disk_cache::ResourceType resource_type = (disk_cache::ResourceType)i;
      disk_cache::settings::SetQuota(resource_type, quotas[resource_type]);
    }
    return;
  }

  // Sum of quotas exceeds |max_bytes|. Set quotas to default values.
  for (int i = 0; i < disk_cache::kTypeCount; i++) {
    disk_cache::ResourceType resource_type = (disk_cache::ResourceType)i;
    uint32_t default_quota = disk_cache::defaults::GetQuota(resource_type);
    disk_cache::settings::SetQuota(resource_type, default_quota);
    std::string directory =
        disk_cache::defaults::GetSubdirectory(resource_type);
    settings->SetPersistentSetting(
        directory,
        std::make_unique<base::Value>(static_cast<double>(default_quota)));
  }
}

uint32_t GetKey(const std::string& s) {
  return starboard::MurmurHash2_32(s.c_str(), s.size());
}

net::ProxyConfig CreateCustomProxyConfig(const std::string& proxy_rules) {
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateDirect();
  proxy_config.proxy_rules().ParseFromString(proxy_rules);
  return proxy_config;
}

#if defined(ENABLE_DEBUGGER)
const char kQUICToggleCommand[] = "quic_toggle";
const char kQUICToggleCommandShortHelp[] = "Toggles QUIC support on/off.";
const char kQUICToggleCommandLongHelp[] =
    "Each time this is called, it will toggle whether QUIC support is "
    "enabled or not. The new value will apply for new streams.";
#endif  // defined(ENABLE_DEBUGGER)

class CobaltCookieAccessDelegate : public net::CookieAccessDelegate {
 public:
  bool ShouldTreatUrlAsTrustworthy(const GURL& url) const override {
    // TODO: Consider checking if URL is trustworthy.
    return true;
  }

  net::CookieAccessSemantics GetAccessSemantics(
      const net::CanonicalCookie& cookie) const override {
    return net::CookieAccessSemantics::LEGACY;
  }

  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override {
    return false;
  }

  absl::optional<net::FirstPartySetMetadata>
  ComputeFirstPartySetMetadataMaybeAsync(
      const net::SchemefulSite& site, const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback)
      const override {
    return net::FirstPartySetMetadata();
  }

  absl::optional<base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>>
  FindFirstPartySetEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<
          void(base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>)>
          callback) const override {
    return std::vector<
        std::pair<net::SchemefulSite, net::FirstPartySetEntry>>();
  }
};

}  // namespace

URLRequestContext::URLRequestContext(
    storage::StorageManager* storage_manager, const std::string& custom_proxy,
    bool ignore_certificate_errors,
    scoped_refptr<base::SequencedTaskRunner> network_task_runner,
    persistent_storage::PersistentSettings* persistent_settings,
    std::unique_ptr<net::HttpUserAgentSettings> http_user_agent_settings,
    std::unique_ptr<net::NetworkDelegate> network_delegate)
#if defined(ENABLE_DEBUGGER)
    : ALLOW_THIS_IN_INITIALIZER_LIST(quic_toggle_command_handler_(
          kQUICToggleCommand,
          base::Bind(&URLRequestContext::OnQuicToggle, base::Unretained(this)),
          kQUICToggleCommandShortHelp, kQUICToggleCommandLongHelp))
#endif  // defined(ENABLE_DEBUGGER)
{
  auto url_request_context_builder =
      std::make_unique<net::URLRequestContextBuilder>();
  url_request_context_builder->set_http_user_agent_settings(
      std::move(http_user_agent_settings));
  url_request_context_builder->set_network_delegate(
      std::move(network_delegate));
  if (storage_manager) {
    persistent_cookie_store_ =
        new PersistentCookieStore(storage_manager, network_task_runner);
  }

  {
    auto cookie_store = std::make_unique<net::CookieMonster>(
        persistent_cookie_store_,
        base::TimeDelta::FromInternalValue(0) /* channel_id_service */,
        net::NetLog::Get());
    cookie_store->SetCookieAccessDelegate(
        std::make_unique<CobaltCookieAccessDelegate>());
    url_request_context_builder->SetCookieStore(std::move(cookie_store));
  }

  url_request_context_builder->set_enable_brotli(true);
  base::Optional<net::ProxyConfig> proxy_config;
  if (!custom_proxy.empty()) {
    proxy_config = CreateCustomProxyConfig(custom_proxy);
  }

  url_request_context_builder->set_proxy_resolution_service(
      net::ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::unique_ptr<net::ProxyConfigService>(
              new ProxyConfigService(proxy_config)),
          net::NetLog::Get(), /*quick_check_enabled=*/true));

#if !defined(QUIC_DISABLED_FOR_STARBOARD)
#ifndef COBALT_PENDING_CLEAN_UP
  // TODO: Confirm this is not needed.
  // ack decimation significantly increases download bandwidth on low-end
  // android devices.
  SetQuicFlag(&FLAGS_quic_reloadable_flag_quic_enable_ack_decimation, true);
#endif
  bool quic_enabled =
      configuration::Configuration::GetInstance()->CobaltEnableQuic();
  if (quic_enabled) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    quic_enabled = !command_line->HasSwitch(switches::kDisableQuic);
  }
#else
  bool quic_enabled = false;
#endif
  url_request_context_builder->SetSpdyAndQuicEnabled(/*spdy_enabled=*/true,
                                                     quic_enabled);

  net::HttpNetworkSessionParams params;
  params.enable_quic = quic_enabled;
  params.use_quic_for_unknown_origins = quic_enabled;

#if defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)
  params.ignore_certificate_errors = ignore_certificate_errors;
  if (ignore_certificate_errors) {
    LOG(INFO) << "ignore_certificate_errors option specified, Certificate "
                 "validation results will be ignored but error message will "
                 "still be displayed.";
  }
#endif  // defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)

  url_request_context_builder->set_http_network_session_params(params);

  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    using_http_cache_ = false;
  } else {
    using_http_cache_ = true;

    int max_cache_bytes = 24 * 1024 * 1024;
#if SB_API_VERSION >= 14
    max_cache_bytes = kSbMaxSystemPathCacheDirectorySize;
#endif
    // Assume the non-http-cache memory in kSbSystemPathCacheDirectory
    // is less than 1 mb and subtract this from the max_cache_bytes.
    max_cache_bytes -= (1 << 20);

    // Initialize and read caching persistent settings
    cache_persistent_settings_ =
        std::make_unique<cobalt::persistent_storage::PersistentSettings>(
            kPersistentSettingsJson);
    LoadDiskCacheQuotaSettings(cache_persistent_settings_.get(),
                               max_cache_bytes);

    // Disable default http cache to use the one created by the supplied
    // callback. |HttpCache| is an |HttpTransactionFactory| with an underlying
    // |HttpTransactionFactory|.
    //
    // In |URLRequestContextBuilder|, first the |http_transaction_factory| is
    // assigned by either the supplied creation callback, a test factory, or
    // a default one.
    //
    // Then |URLRequestContextBuilder.http_cache_enabled_| is checked. When
    // |true|, the |http_transaction_factory| is replaced with a default
    // |HttpCache| wrapping the previously assign |http_transaction_factory|.
    //
    // We want to use the |HttpCache| created by the supplied callback, and we
    // do not want it wrapped by the default |HttpCache|.
    url_request_context_builder->DisableHttpCache();
    url_request_context_builder->SetCreateHttpTransactionFactoryCallback(
        base::BindOnce(
            [](persistent_storage::PersistentSettings* persistent_settings,
               int max_cache_bytes, const std::vector<char>& path,
               URLRequestContext* url_request_context,
               net::HttpNetworkSession* session)
                -> std::unique_ptr<net::HttpTransactionFactory> {
              auto http_cache = std::make_unique<net::HttpCache>(
                  std::make_unique<net::HttpNetworkLayer>(session),
                  std::make_unique<disk_cache::CobaltBackendFactory>(
                      base::FilePath(std::string(path.data())),
                      /* max_bytes */ max_cache_bytes, url_request_context));
              http_cache->set_can_disable_by_mime_type(true);
              if (persistent_settings != nullptr) {
                auto cache_enabled =
                    persistent_settings->GetPersistentSettingAsBool(
                        disk_cache::kCacheEnabledPersistentSettingsKey, true);
                disk_cache::settings::SetCacheEnabled(cache_enabled);
                if (!cache_enabled) {
                  http_cache->set_mode(net::HttpCache::Mode::DISABLE);
                }
              }
              return http_cache;
            },
            base::Unretained(persistent_settings), max_cache_bytes, path,
            base::Unretained(this)));
  }

#ifndef COBALT_PENDING_CLEAN_UP
  // TODO: Determine if this is needed.
  url_request_context_builder->SetProtocolHandler(
      url::kDataScheme, std::make_unique<net::DataProtocolHandler>());
#endif

  url_request_context_ = url_request_context_builder->Build();
}

URLRequestContext::~URLRequestContext() {}

std::unique_ptr<net::URLRequest> URLRequestContext::CreateRequest(
    const GURL& url, net::RequestPriority priority,
    net::URLRequest::Delegate* delegate) {
  return url_request_context_->CreateRequest(url, priority, delegate);
}

void URLRequestContext::SetProxy(const std::string& proxy_rules) {
  net::ProxyConfig proxy_config = CreateCustomProxyConfig(proxy_rules);
  // ProxyService takes ownership of the ProxyConfigService.
  url_request_context_->set_proxy_resolution_service(
      net::ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::make_unique<ProxyConfigService>(proxy_config),
          net::NetLog::Get(),
          /*quick_check_enabled=*/true));
}

void URLRequestContext::SetEnableQuic(bool enable_quic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_request_context_->http_network_session()->SetEnableQuic(enable_quic);
}

bool URLRequestContext::using_http_cache() { return using_http_cache_; }

#if defined(ENABLE_DEBUGGER)
void URLRequestContext::OnQuicToggle(const std::string& message) {
  DCHECK(url_request_context_->http_network_session());
  SetEnableQuic(!url_request_context_->http_network_session()->IsQuicEnabled());
}
#endif  // defined(ENABLE_DEBUGGER)

void URLRequestContext::UpdateCacheSizeSetting(disk_cache::ResourceType type,
                                               uint32_t bytes) {
  CHECK(cache_persistent_settings_);
  cache_persistent_settings_->SetPersistentSetting(
      disk_cache::defaults::GetSubdirectory(type),
      std::make_unique<base::Value>(static_cast<double>(bytes)));
}

void URLRequestContext::ValidateCachePersistentSettings() {
  cache_persistent_settings_->ValidatePersistentSettings();
}

void URLRequestContext::AssociateKeyWithResourceType(
    const std::string& key, disk_cache::ResourceType resource_type) {
  url_resource_type_map_[GetKey(key)] = resource_type;
}

disk_cache::ResourceType URLRequestContext::GetType(const std::string& key) {
  uint32_t uint_key = GetKey(key);
  if (url_resource_type_map_.count(uint_key) == 0) {
    return disk_cache::kOther;
  }
  return url_resource_type_map_[uint_key];
}

}  // namespace network
}  // namespace cobalt
