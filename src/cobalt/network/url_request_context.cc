/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/network/url_request_context.h"

#include <string>

#include "base/threading/worker_pool.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/persistent_cookie_store.h"
#include "cobalt/network/proxy_config_service.h"
#include "net/base/cert_verify_proc.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_cache.h"
#include "net/base/multi_threaded_cert_verifier.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/ssl_config_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_source.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace cobalt {
namespace network {
namespace {
net::ProxyConfig CreateCustomProxyConfig(const std::string& proxy_rules) {
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateDirect();
  proxy_config.set_source(net::PROXY_CONFIG_SOURCE_CUSTOM);
  proxy_config.proxy_rules().ParseFromString(proxy_rules);
  return proxy_config;
}
}  // namespace

URLRequestContext::URLRequestContext(storage::StorageManager* storage_manager,
                                     const std::string& custom_proxy,
                                     net::NetLog* net_log,
                                     bool ignore_certificate_errors)
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  if (storage_manager) {
    persistent_cookie_store_ = new PersistentCookieStore(storage_manager);
  }
  storage_.set_cookie_store(
      new net::CookieMonster(persistent_cookie_store_, NULL /* delegate */));
  storage_.set_server_bound_cert_service(new net::ServerBoundCertService(
      new net::DefaultServerBoundCertStore(NULL),
      base::WorkerPool::GetTaskRunner(true)));

  base::optional<net::ProxyConfig> proxy_config;
  if (!custom_proxy.empty()) {
    proxy_config = CreateCustomProxyConfig(custom_proxy);
  }

  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      new ProxyConfigService(proxy_config));
  storage_.set_proxy_service(net::ProxyService::CreateWithoutProxyResolver(
      proxy_config_service.release(), NULL));

  net::HostResolver::Options options;
  options.max_concurrent_resolves = net::HostResolver::kDefaultParallelism;
  options.max_retry_attempts = net::HostResolver::kDefaultRetryAttempts;
  options.enable_caching = true;
  storage_.set_host_resolver(
      net::HostResolver::CreateSystemResolver(options, NULL));

  storage_.set_cert_verifier(
      new net::MultiThreadedCertVerifier(net::CertVerifyProc::CreateDefault()));
  storage_.set_ssl_config_service(new net::SSLConfigServiceDefaults);

  storage_.set_http_auth_handler_factory(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  storage_.set_http_server_properties(new net::HttpServerPropertiesImpl);

  net::HttpNetworkSession::Params params;
  params.client_socket_factory = NULL;
  params.host_resolver = host_resolver();
  params.cert_verifier = cert_verifier();
  params.server_bound_cert_service = server_bound_cert_service();
  params.transport_security_state = NULL;
  params.proxy_service = proxy_service();
  params.ssl_session_cache_shard = "";
  params.ssl_config_service = ssl_config_service();
  params.http_auth_handler_factory = http_auth_handler_factory();
  params.network_delegate = NULL;
  params.http_server_properties = http_server_properties();
#if defined(ENABLE_NETWORK_LOGGING)
  params.net_log = net_log;
  set_net_log(net_log);
#else
  UNREFERENCED_PARAMETER(net_log);
#endif
  params.trusted_spdy_proxy = "";
#if defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)
  params.ignore_certificate_errors = ignore_certificate_errors;
#else
  UNREFERENCED_PARAMETER(ignore_certificate_errors);
#endif  // defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)

  // disable caching
  net::HttpCache* cache = new net::HttpCache(params, NULL /* backend */);
  cache->set_mode(net::HttpCache::DISABLE);
  storage_.set_http_transaction_factory(cache);

  storage_.set_job_factory(new net::URLRequestJobFactoryImpl());
}

URLRequestContext::~URLRequestContext() {}

void URLRequestContext::SetProxy(const std::string& proxy_rules) {
  net::ProxyConfig proxy_config = CreateCustomProxyConfig(proxy_rules);
  // ProxyService takes ownership of the ProxyConfigService.
  proxy_service()->ResetConfigService(new ProxyConfigService(proxy_config));
}

}  // namespace network
}  // namespace cobalt
