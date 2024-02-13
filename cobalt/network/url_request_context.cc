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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/configuration/configuration.h"
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
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
namespace {

const char kPersistentSettingsJson[] = "cache_settings.json";


void ReadDiskCacheSize(cobalt::persistent_storage::PersistentSettings* settings,
                       int64_t max_bytes) {
#ifndef USE_HACKY_COBALT_CHANGES
  auto total_size = 0;
  cobalt::network::disk_cache::ResourceTypeMetadata
      kTypeMetadataNew[cobalt::network::disk_cache::kTypeCount];

  for (int i = 0; i < disk_cache::kTypeCount; i++) {
    auto metadata = cobalt::network::disk_cache::kTypeMetadata[i];
    uint32_t bucket_size =
        static_cast<uint32_t>(settings->GetPersistentSettingAsDouble(
            metadata.directory, metadata.max_size_bytes));
    kTypeMetadataNew[i] = {metadata.directory, bucket_size};

    total_size += bucket_size;
  }

  // Check if PersistentSettings values are valid and can replace the
  // disk_cache::kTypeMetadata.
  if (total_size <= max_bytes) {
    std::copy(std::begin(kTypeMetadataNew), std::end(kTypeMetadataNew),
              std::begin(disk_cache::kTypeMetadata));
    return;
  }

  // PersistentSettings values are invalid and will be replaced by the
  default
      // values in disk_cache::kTypeMetadata.
      for (int i = 0; i < disk_cache::kTypeCount; i++) {
    auto metadata = disk_cache::kTypeMetadata[i];
    settings->SetPersistentSetting(
        metadata.directory, std::make_unique<base::Value>(
                                static_cast<double>(metadata.max_size_bytes)));
  }
}
#else
}
#endif
}  // namespace

namespace cobalt {
namespace network {
namespace {
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

}  // namespace

URLRequestContext::URLRequestContext(
    storage::StorageManager* storage_manager, const std::string& custom_proxy,
    net::NetLog* net_log, bool ignore_certificate_errors,
    scoped_refptr<base::SequencedTaskRunner> network_task_runner,
    persistent_storage::PersistentSettings* persistent_settings)
#if defined(ENABLE_DEBUGGER)
    : ALLOW_THIS_IN_INITIALIZER_LIST(quic_toggle_command_handler_(
          kQUICToggleCommand,
          base::Bind(&URLRequestContext::OnQuicToggle, base::Unretained(this)),
          kQUICToggleCommandShortHelp, kQUICToggleCommandLongHelp))
#endif  // defined(ENABLE_DEBUGGER)
{
  if (storage_manager) {
    persistent_cookie_store_ =
        new PersistentCookieStore(storage_manager, network_task_runner);
  }
#ifndef USE_HACKY_COBALT_CHANGES
  set_cookie_store(std::unique_ptr<net::CookieStore>(new net::CookieMonster(
      persistent_cookie_store_,
      base::TimeDelta::FromInternalValue(0) /* channel_id_service */,
      net_log)));

  set_enable_brotli(true);
#endif

  base::Optional<net::ProxyConfig> proxy_config;
  if (!custom_proxy.empty()) {
    proxy_config = CreateCustomProxyConfig(custom_proxy);
  }

#ifndef USE_HACKY_COBALT_CHANGES
  set_proxy_resolution_service(
      net::ProxyResolutionService::CreateUsingSystemProxyResolver(
          std::unique_ptr<net::ProxyConfigService>(
              new ProxyConfigService(proxy_config)),
          net_log));
#endif

  // ack decimation significantly increases download bandwidth on low-end
  // android devices.
#ifndef USE_HACKY_COBALT_CHANGES
  SetQuicFlag(&FLAGS_quic_reloadable_flag_quic_enable_ack_decimation, true);
#endif

  net::HostResolver::ManagerOptions options;
#ifndef USE_HACKY_COBALT_CHANGES
  options.enable_caching = true;
  set_host_resolver(
      net::HostResolver::CreateStandaloneResolver(nullptr, options));

  set_ct_policy_enforcer(std::unique_ptr<net::CTPolicyEnforcer>(
      new net::DefaultCTPolicyEnforcer()));
  DCHECK(ct_policy_enforcer());
  // As of Chromium m70 net, CreateDefault will return a caching multi-thread
  // cert verifier, the verification cache will usually cache 25-40
  // results in a single session which can take up to 100KB memory.
  set_cert_verifier(net::CertVerifier::CreateDefault());
  set_transport_security_state(std::make_unique<net::TransportSecurityState>());
  // TODO: Investigate if we want the cert transparency verifier.
  set_cert_transparency_verifier(std::make_unique<net::DoNothingCTVerifier>());
  set_ssl_config_service(std::make_unique<net::SSLConfigServiceDefaults>());

  set_http_auth_handler_factory(net::HttpAuthHandlerFactory::CreateDefault());
  set_http_server_properties(std::make_unique<net::HttpServerPropertiesImpl>());
#endif

  net::HttpNetworkSessionParams params;

  if (configuration::Configuration::GetInstance()->CobaltEnableQuic()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    params.enable_quic = !command_line->HasSwitch(switches::kDisableQuic);
#ifndef USE_HACKY_COBALT_CHANGES
    params.use_quic_for_unknown_origins = params.enable_quic;
#endif
  }
#if defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)
#ifndef USE_HACKY_COBALT_CHANGES
  params.ignore_certificate_errors = ignore_certificate_errors;
  if (ignore_certificate_errors) {
    cert_verifier()->set_ignore_certificate_errors(true);
    LOG(INFO) << "ignore_certificate_errors option specified, Certificate "
                 "validation results will be ignored but error message will "
                 "still be displayed.";
  }
#endif
#endif  // defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)

  net::HttpNetworkSessionContext context;
  context.client_socket_factory = nullptr;
#ifndef USE_HACKY_COBALT_CHANGES
  context.host_resolver = host_resolver();
  context.cert_verifier = cert_verifier();
  context.ct_policy_enforcer = ct_policy_enforcer();
  context.channel_id_service = NULL;
  context.transport_security_state = transport_security_state();
  // context.cert_transparency_verifier = cert_transparency_verifier();
  context.proxy_resolution_service = proxy_resolution_service();
  context.ssl_config_service = ssl_config_service();
  context.http_auth_handler_factory = http_auth_handler_factory();
  context.http_server_properties = http_server_properties();
#endif
#if defined(ENABLE_NETWORK_LOGGING)
  context.net_log = net_log;
#ifndef USE_HACKY_COBALT_CHANGES
  set_net_log(net_log);
#endif
#else
#endif
  context.socket_performance_watcher_factory = nullptr;
#ifndef USE_HACKY_COBALT_CHANGES
  context.network_quality_provider = NULL;

  set_http_network_session(
      std::make_unique<net::HttpNetworkSession>(params, context));
#endif
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
#ifndef USE_HACKY_COBALT_CHANGES
    set_http_transaction_factory(std::unique_ptr<net::HttpNetworkLayer>(
        new net::HttpNetworkLayer(http_network_session_)));
#endif
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
#ifndef USE_HACKY_COBALT_CHANGES
    cache_persistent_settings_ =
        std::make_unique<cobalt::persistent_storage::PersistentSettings>(
            kPersistentSettingsJson);
    ReadDiskCacheSize(cache_persistent_settings_.get(), max_cache_bytes);

    auto http_cache = std::make_unique<net::HttpCache>(
        http_network_session(),
        std::make_unique<net::HttpCache::DefaultBackend>(
            net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, nullptr,
            base::FilePath(std::string(path.data())),
            /* max_bytes */ max_cache_bytes, false),
        true);
    if (persistent_settings != nullptr) {
      auto cache_enabled = persistent_settings->GetPersistentSettingAsBool(
          disk_cache::kCacheEnabledPersistentSettingsKey, true);

      if (!cache_enabled) {
        http_cache->set_mode(net::HttpCache::Mode::DISABLE);
      }
    }

    set_http_transaction_factory(std::move(http_cache));
#endif
  }

  auto* job_factory = new net::URLRequestJobFactory();
#ifndef USE_HACKY_COBALT_CHANGES
  job_factory->SetProtocolHandler(url::kDataScheme,
                                  std::make_unique<net::DataProtocolHandler>());
#endif

#if defined(ENABLE_CONFIGURE_REQUEST_JOB_FACTORY)
  ConfigureRequestJobFactory(job_factory);
#endif  // defined(ENABLE_CONFIGURE_REQUEST_JOB_FACTORY)

#ifndef USE_HACKY_COBALT_CHANGES
  set_job_factory(std::unique_ptr<net::URLRequestJobFactory>(job_factory));
#endif
}

URLRequestContext::~URLRequestContext() {}

void URLRequestContext::SetProxy(const std::string& proxy_rules) {
  net::ProxyConfig proxy_config = CreateCustomProxyConfig(proxy_rules);
  // ProxyService takes ownership of the ProxyConfigService.
#ifndef USE_HACKY_COBALT_CHANGES
  proxy_resolution_service()->ResetConfigService(
      std::make_unique<ProxyConfigService>(proxy_config));
#endif
}

void URLRequestContext::SetEnableQuic(bool enable_quic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#ifndef USE_HACKY_COBALT_CHANGES
  http_network_session()->SetEnableQuic(enable_quic);
#endif
}

#ifndef USE_HACKY_COBALT_CHANGES
bool URLRequestContext::using_http_cache() { return using_http_cache_; }
#endif

#if defined(ENABLE_DEBUGGER)
void URLRequestContext::OnQuicToggle(const std::string& message) {
#ifndef USE_HACKY_COBALT_CHANGES
  DCHECK(http_network_session());
  http_network_session()->ToggleQuic();
#endif
}
#endif  // defined(ENABLE_DEBUGGER)

void URLRequestContext::UpdateCacheSizeSetting(disk_cache::ResourceType type,
                                               uint32_t bytes) {
#ifndef USE_HACKY_COBALT_CHANGES
  CHECK(cache_persistent_settings_);
  cache_persistent_settings_->SetPersistentSetting(
      disk_cache::kTypeMetadata[type].directory,
      std::make_unique<base::Value>(static_cast<double>(bytes)));
#endif
}

void URLRequestContext::ValidateCachePersistentSettings() {
#ifndef USE_HACKY_COBALT_CHANGES
  cache_persistent_settings_->ValidatePersistentSettings();
#endif
}

}  // namespace network
}  // namespace cobalt
