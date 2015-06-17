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

#include "base/threading/worker_pool.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/persistent_cookie_store.h"
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
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace cobalt {
namespace network {

URLRequestContext::URLRequestContext(storage::StorageManager* storage_manager)
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  if (storage_manager) {
    persistent_cookie_store_ = new PersistentCookieStore(storage_manager);
  }
  storage_.set_cookie_store(
      new net::CookieMonster(persistent_cookie_store_, NULL /* delegate */));
  storage_.set_server_bound_cert_service(new net::ServerBoundCertService(
      new net::DefaultServerBoundCertStore(NULL),
      base::WorkerPool::GetTaskRunner(true)));

  // TODO(***REMOVED***): Respect any command line fixed proxy arguments.
  // TODO(***REMOVED***): Bring over ProxyConfigServiceShell
  scoped_ptr<net::ProxyConfigService> proxy_config_service;
  net::ProxyConfig empty_config;
  proxy_config_service.reset(new net::ProxyConfigServiceFixed(empty_config));
  storage_.set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service.release(), 0, NULL));

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
  params.net_log = NULL;
  params.trusted_spdy_proxy = "";

  // disable caching
  net::HttpCache* cache = new net::HttpCache(params, NULL /* backend */);
  cache->set_mode(net::HttpCache::DISABLE);
  storage_.set_http_transaction_factory(cache);

  storage_.set_job_factory(new net::URLRequestJobFactoryImpl());
}

URLRequestContext::~URLRequestContext() {}

}  // namespace network
}  // namespace cobalt
