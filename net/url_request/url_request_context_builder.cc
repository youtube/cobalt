// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/transport_security_state.h"
#include "net/cookies/cookie_monster.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"

namespace net {

namespace {

class BasicNetworkDelegate : public NetworkDelegate {
 public:
  BasicNetworkDelegate() {}
  virtual ~BasicNetworkDelegate() {}

 private:
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 const CompletionCallback& callback,
                                 GURL* new_url) override {
    return OK;
  }

  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) override {
    return OK;
  }

  virtual void OnSendHeaders(URLRequest* request,
                             const HttpRequestHeaders& headers) override {}

  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers)
      override {
    return OK;
  }

  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) override {}

  virtual void OnResponseStarted(URLRequest* request) override {}

  virtual void OnRawBytesRead(const URLRequest& request,
                              int bytes_read) override {}

  virtual void OnCompleted(URLRequest* request, bool started) override {}

  virtual void OnURLRequestDestroyed(URLRequest* request) override {}

  virtual void OnPACScriptError(int line_number,
                                const string16& error) override {}

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) override {
    return NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  virtual bool OnCanGetCookies(const URLRequest& request,
                               const CookieList& cookie_list) override {
    return true;
  }

  virtual bool OnCanSetCookie(const URLRequest& request,
                              const std::string& cookie_line,
                              CookieOptions* options) override {
    return true;
  }

  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const FilePath& path) const override {
    return true;
  }

  virtual bool OnCanThrottleRequest(const URLRequest& request) const override {
    return false;
  }

  virtual int OnBeforeSocketStreamConnect(
      SocketStream* stream,
      const CompletionCallback& callback) override {
    return OK;
  }

  virtual void OnRequestWaitStateChange(const URLRequest& request,
                                        RequestWaitState state) override {
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

class BasicURLRequestContext : public URLRequestContext {
 public:
  BasicURLRequestContext()
      : cache_thread_("Cache Thread"),
        file_thread_("File Thread"),
        ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {}

  URLRequestContextStorage* storage() {
    return &storage_;
  }

  void StartCacheThread() {
    cache_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, 0));
  }

  scoped_refptr<base::MessageLoopProxy> cache_message_loop_proxy() {
    DCHECK(cache_thread_.IsRunning());
    return cache_thread_.message_loop_proxy();
  }

  void StartFileThread() {
    file_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_DEFAULT, 0));
  }

  MessageLoop* file_message_loop() {
    DCHECK(file_thread_.IsRunning());
    return file_thread_.message_loop();
  }

 protected:
  virtual ~BasicURLRequestContext() {}

 private:
  base::Thread cache_thread_;
  base::Thread file_thread_;
  URLRequestContextStorage storage_;
  DISALLOW_COPY_AND_ASSIGN(BasicURLRequestContext);
};

}  // namespace

URLRequestContextBuilder::HttpCacheParams::HttpCacheParams()
    : type(IN_MEMORY),
      max_size(0) {}
URLRequestContextBuilder::HttpCacheParams::~HttpCacheParams() {}

URLRequestContextBuilder::HttpNetworkSessionParams::HttpNetworkSessionParams()
    : ignore_certificate_errors(false),
      host_mapping_rules(NULL),
      http_pipelining_enabled(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      trusted_spdy_proxy() {}

URLRequestContextBuilder::HttpNetworkSessionParams::~HttpNetworkSessionParams()
{}

URLRequestContextBuilder::URLRequestContextBuilder()
    : ftp_enabled_(false),
      http_cache_enabled_(true) {}
URLRequestContextBuilder::~URLRequestContextBuilder() {}

#if defined(OS_LINUX) || defined(OS_ANDROID)
void URLRequestContextBuilder::set_proxy_config_service(
    ProxyConfigService* proxy_config_service) {
  proxy_config_service_.reset(proxy_config_service);
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

URLRequestContext* URLRequestContextBuilder::Build() {
#if __LB_ENABLE_NATIVE_HTTP_STACK__
  return NULL;
#else
  BasicURLRequestContext* context = new BasicURLRequestContext;
  URLRequestContextStorage* storage = context->storage();

  storage->set_http_user_agent_settings(new StaticHttpUserAgentSettings(
      accept_language_, accept_charset_, user_agent_));

  if (!network_delegate_)
    network_delegate_.reset(new BasicNetworkDelegate);
  storage->set_network_delegate(network_delegate_.release());

  storage->set_host_resolver(net::HostResolver::CreateDefaultResolver(NULL));

#if !DISABLE_FTP_SUPPORT
  if (ftp_enabled_) {
    storage->set_ftp_transaction_factory(
        new FtpNetworkLayer(context->host_resolver()));
  }
#endif

  context->StartFileThread();

  // TODO(willchan): Switch to using this code when
  // ProxyService::CreateSystemProxyConfigService()'s signature doesn't suck.
#if defined(OS_LINUX) || defined(OS_ANDROID)
  ProxyConfigService* proxy_config_service = proxy_config_service_.release();
#else
  ProxyConfigService* proxy_config_service =
      ProxyService::CreateSystemProxyConfigService(
          base::ThreadTaskRunnerHandle::Get(),
          context->file_message_loop());
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
  storage->set_proxy_service(
      ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service,
          4,  // TODO(willchan): Find a better constant somewhere.
          context->net_log()));
  storage->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  storage->set_http_auth_handler_factory(
      net::HttpAuthHandlerRegistryFactory::CreateDefault(
          context->host_resolver()));
  storage->set_cookie_store(new CookieMonster(NULL, NULL));
  storage->set_transport_security_state(new net::TransportSecurityState());
  storage->set_http_server_properties(new net::HttpServerPropertiesImpl);
  storage->set_cert_verifier(CertVerifier::CreateDefault());

  net::HttpNetworkSession::Params network_session_params;
  network_session_params.host_resolver = context->host_resolver();
  network_session_params.cert_verifier = context->cert_verifier();
  network_session_params.transport_security_state =
      context->transport_security_state();
  network_session_params.proxy_service = context->proxy_service();
  network_session_params.ssl_config_service =
      context->ssl_config_service();
  network_session_params.http_auth_handler_factory =
      context->http_auth_handler_factory();
  network_session_params.network_delegate =
      context->network_delegate();
  network_session_params.http_server_properties =
      context->http_server_properties();
  network_session_params.net_log = context->net_log();
  network_session_params.ignore_certificate_errors =
      http_network_session_params_.ignore_certificate_errors;
  network_session_params.host_mapping_rules =
      http_network_session_params_.host_mapping_rules;
  network_session_params.http_pipelining_enabled =
      http_network_session_params_.http_pipelining_enabled;
  network_session_params.testing_fixed_http_port =
      http_network_session_params_.testing_fixed_http_port;
  network_session_params.testing_fixed_https_port =
      http_network_session_params_.testing_fixed_https_port;
  network_session_params.trusted_spdy_proxy =
      http_network_session_params_.trusted_spdy_proxy;

  HttpTransactionFactory* http_transaction_factory = NULL;
  if (http_cache_enabled_) {
    network_session_params.server_bound_cert_service =
        context->server_bound_cert_service();
    HttpCache::BackendFactory* http_cache_backend = NULL;
    if (http_cache_params_.type == HttpCacheParams::DISK) {
      context->StartCacheThread();
      http_cache_backend =
          new HttpCache::DefaultBackend(DISK_CACHE,
                                        http_cache_params_.path,
                                        http_cache_params_.max_size,
                                        context->cache_message_loop_proxy());
    } else {
      http_cache_backend =
          HttpCache::DefaultBackend::InMemory(http_cache_params_.max_size);
    }

    http_transaction_factory = new HttpCache(
        network_session_params, http_cache_backend);
  } else {
    scoped_refptr<net::HttpNetworkSession> network_session(
        new net::HttpNetworkSession(network_session_params));

    http_transaction_factory = new HttpNetworkLayer(network_session);
  }
  storage->set_http_transaction_factory(http_transaction_factory);

  // TODO(willchan): Support sdch.

  return context;
#endif
}

}  // namespace net
