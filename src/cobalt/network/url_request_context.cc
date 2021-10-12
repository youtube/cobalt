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

#include <memory>
#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/network/job_factory_config.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/persistent_cookie_store.h"
#include "cobalt/network/proxy_config_service.h"
#include "cobalt/network/switches.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert_net/cert_net_fetcher_impl.h"
#include "net/dns/host_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/url_request_job_factory_impl.h"

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
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this))
#if defined(ENABLE_DEBUGGER)
      ,
      ALLOW_THIS_IN_INITIALIZER_LIST(quic_toggle_command_handler_(
          kQUICToggleCommand,
          base::Bind(&URLRequestContext::OnQuicToggle, base::Unretained(this)),
          kQUICToggleCommandShortHelp, kQUICToggleCommandLongHelp))
#endif  // defined(ENABLE_DEBUGGER)
{
  if (storage_manager) {
    persistent_cookie_store_ =
        new PersistentCookieStore(storage_manager, network_task_runner);
  }
  storage_.set_cookie_store(
      std::unique_ptr<net::CookieStore>(new net::CookieMonster(
          persistent_cookie_store_, NULL /* channel_id_service */, net_log)));

  set_enable_brotli(true);

  base::Optional<net::ProxyConfig> proxy_config;
  if (!custom_proxy.empty()) {
    proxy_config = CreateCustomProxyConfig(custom_proxy);
  }

  storage_.set_proxy_resolution_service(
      net::ProxyResolutionService::CreateUsingSystemProxyResolver(
          std::unique_ptr<net::ProxyConfigService>(
              new ProxyConfigService(proxy_config)),
          net_log));

  // ack decimation significantly increases download bandwidth on low-end
  // android devices.
  SetQuicFlag(&FLAGS_quic_reloadable_flag_quic_enable_ack_decimation, true);

  net::HostResolver::Options options;
  options.max_concurrent_resolves = net::HostResolver::kDefaultParallelism;
  options.max_retry_attempts = net::HostResolver::kDefaultRetryAttempts;
  options.enable_caching = true;
  storage_.set_host_resolver(
      net::HostResolver::CreateSystemResolver(options, NULL));

  storage_.set_ct_policy_enforcer(std::unique_ptr<net::CTPolicyEnforcer>(
      new net::DefaultCTPolicyEnforcer()));
  DCHECK(ct_policy_enforcer());
  // As of Chromium m70 net, CreateDefault will return a caching multi-thread
  // cert verifier, the verification cache will usually cache 25-40
  // results in a single session which can take up to 100KB memory.
  storage_.set_cert_verifier(net::CertVerifier::CreateDefault());
  storage_.set_transport_security_state(
      std::make_unique<net::TransportSecurityState>());
  // TODO: Investigate if we want the cert transparency verifier.
  storage_.set_cert_transparency_verifier(
      std::make_unique<net::DoNothingCTVerifier>());
  storage_.set_ssl_config_service(
      std::make_unique<net::SSLConfigServiceDefaults>());

  storage_.set_http_auth_handler_factory(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  storage_.set_http_server_properties(
      std::make_unique<net::HttpServerPropertiesImpl>());

  net::HttpNetworkSession::Params params;

  if (configuration::Configuration::GetInstance()->CobaltEnableQuic()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    params.enable_quic = !command_line->HasSwitch(switches::kDisableQuic);
    params.use_quic_for_unknown_origins = params.enable_quic;
  }
#if defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)
  params.ignore_certificate_errors = ignore_certificate_errors;
  if (ignore_certificate_errors) {
    cert_verifier()->set_ignore_certificate_errors(true);
    LOG(INFO) << "ignore_certificate_errors option specified, Certificate "
                 "validation results will be ignored but error message will "
                 "still be displayed.";
  }
#endif  // defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)

  net::HttpNetworkSession::Context context;
  context.client_socket_factory = NULL;
  context.host_resolver = host_resolver();
  context.cert_verifier = cert_verifier();
  context.ct_policy_enforcer = ct_policy_enforcer();
  context.channel_id_service = NULL;
  context.transport_security_state = transport_security_state();
  context.cert_transparency_verifier = cert_transparency_verifier();
  context.proxy_resolution_service = proxy_resolution_service();
  context.ssl_config_service = ssl_config_service();
  context.http_auth_handler_factory = http_auth_handler_factory();
  context.http_server_properties = http_server_properties();
#if defined(ENABLE_NETWORK_LOGGING)
  context.net_log = net_log;
  set_net_log(net_log);
#else
#endif
  context.socket_performance_watcher_factory = NULL;
  context.network_quality_provider = NULL;

  storage_.set_http_network_session(
      std::make_unique<net::HttpNetworkSession>(params, context));
  storage_.set_http_transaction_factory(std::unique_ptr<net::HttpNetworkLayer>(
      new net::HttpNetworkLayer(storage_.http_network_session())));

  auto* job_factory = new net::URLRequestJobFactoryImpl();
  job_factory->SetProtocolHandler(url::kDataScheme,
                                  std::make_unique<net::DataProtocolHandler>());

#if defined(ENABLE_CONFIGURE_REQUEST_JOB_FACTORY)
  ConfigureRequestJobFactory(job_factory);
#endif  // defined(ENABLE_CONFIGURE_REQUEST_JOB_FACTORY)

  storage_.set_job_factory(
      std::unique_ptr<net::URLRequestJobFactory>(job_factory));
}

URLRequestContext::~URLRequestContext() {}

void URLRequestContext::SetProxy(const std::string& proxy_rules) {
  net::ProxyConfig proxy_config = CreateCustomProxyConfig(proxy_rules);
  // ProxyService takes ownership of the ProxyConfigService.
  proxy_resolution_service()->ResetConfigService(
      std::make_unique<ProxyConfigService>(proxy_config));
}

void URLRequestContext::SetEnableQuic(bool enable_quic) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  storage_.http_network_session()->SetEnableQuic(enable_quic);
}

#if defined(ENABLE_DEBUGGER)
void URLRequestContext::OnQuicToggle(const std::string& message) {
  DCHECK(storage_.http_network_session());
  storage_.http_network_session()->ToggleQuic();
}
#endif  // defined(ENABLE_DEBUGGER)

}  // namespace network
}  // namespace cobalt
