// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread.h"
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
                                 GURL* new_url) OVERRIDE {
    return OK;
  }

  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) OVERRIDE {
    return OK;
  }

  virtual void OnSendHeaders(URLRequest* request,
                             const HttpRequestHeaders& headers) OVERRIDE {}

  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers)
      OVERRIDE {
    return OK;
  }

  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) OVERRIDE {}

  virtual void OnResponseStarted(URLRequest* request) OVERRIDE {}

  virtual void OnRawBytesRead(const URLRequest& request,
                              int bytes_read) OVERRIDE {}

  virtual void OnCompleted(URLRequest* request, bool started) OVERRIDE {}

  virtual void OnURLRequestDestroyed(URLRequest* request) OVERRIDE {}

  virtual void OnPACScriptError(int line_number,
                                const string16& error) OVERRIDE {}

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) OVERRIDE {
    return NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  virtual bool OnCanGetCookies(const URLRequest& request,
                               const CookieList& cookie_list) OVERRIDE {
    return true;
  }

  virtual bool OnCanSetCookie(const URLRequest& request,
                              const std::string& cookie_line,
                              CookieOptions* options) OVERRIDE {
    return true;
  }

  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const FilePath& path) const OVERRIDE {
    return true;
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

  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
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

  virtual const std::string& GetUserAgent(
      const GURL& /* url */) const OVERRIDE {
    return user_agent_;
  }

 protected:
  virtual ~BasicURLRequestContext() {}

 private:
  std::string user_agent_;
  base::Thread cache_thread_;
  base::Thread file_thread_;
  URLRequestContextStorage storage_;
  DISALLOW_COPY_AND_ASSIGN(BasicURLRequestContext);
};

}  // namespace

URLRequestContextBuilder::HostResolverParams::HostResolverParams()
    : parallelism(HostResolver::kDefaultParallelism),
      retry_attempts(HostResolver::kDefaultRetryAttempts) {}
URLRequestContextBuilder::HostResolverParams::~HostResolverParams() {}

URLRequestContextBuilder::HttpCacheParams::HttpCacheParams()
    : type(DISK),
      max_size(0),
      path(FILE_PATH_LITERAL("Cache")) {}
URLRequestContextBuilder::HttpCacheParams::~HttpCacheParams() {}

URLRequestContextBuilder::URLRequestContextBuilder()
    : ftp_enabled_(false),
      http_cache_enabled_(true) {}
URLRequestContextBuilder::~URLRequestContextBuilder() {}

#if defined(OS_LINUX)
void URLRequestContextBuilder::set_proxy_config_service(
    ProxyConfigService* proxy_config_service) {
  proxy_config_service_.reset(proxy_config_service);
}
#endif  // defined(OS_LINUX)

URLRequestContext* URLRequestContextBuilder::Build() {
  BasicURLRequestContext* context = new BasicURLRequestContext;
  URLRequestContextStorage* storage = context->storage();

  context->set_user_agent(user_agent_);

  BasicNetworkDelegate* network_delegate = new BasicNetworkDelegate;
  storage->set_network_delegate(network_delegate);

  net::HostResolver* host_resolver = net::CreateSystemHostResolver(
      host_resolver_params_.parallelism,
      host_resolver_params_.retry_attempts,
      NULL /* no NetLog */);
  storage->set_host_resolver(host_resolver);

  if (ftp_enabled_) {
    storage->set_ftp_transaction_factory(new FtpNetworkLayer(host_resolver));
  }

  context->StartFileThread();

  // TODO(willchan): Switch to using this code when
  // ProxyService::CreateSystemProxyConfigService()'s signature doesn't suck.
#if defined(OS_LINUX)
  ProxyConfigService* proxy_config_service = proxy_config_service_.release();
#else
  ProxyConfigService* proxy_config_service =
      ProxyService::CreateSystemProxyConfigService(
          MessageLoop::current(),
          context->file_message_loop());
#endif  // defined(OS_LINUX)
  storage->set_proxy_service(
      ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service,
          4,  // TODO(willchan): Find a better constant somewhere.
          context->net_log()));
  storage->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  storage->set_http_auth_handler_factory(
      net::HttpAuthHandlerRegistryFactory::CreateDefault(host_resolver));
  storage->set_cookie_store(new CookieMonster(NULL, NULL));
  storage->set_transport_security_state(new net::TransportSecurityState());
  storage->set_http_server_properties(new net::HttpServerPropertiesImpl);
  storage->set_cert_verifier(CertVerifier::CreateDefault());

  HttpTransactionFactory* http_transaction_factory = NULL;
  if (http_cache_enabled_) {
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
        context->host_resolver(),
        context->cert_verifier(),
        context->server_bound_cert_service(),
        context->transport_security_state(),
        context->proxy_service(),
        "",
        context->ssl_config_service(),
        context->http_auth_handler_factory(),
        context->network_delegate(),
        context->http_server_properties(),
        context->net_log(),
        http_cache_backend,
        "" /* trusted_spdy_proxy */ );
  } else {
    HttpNetworkSession::Params session_params;
    session_params.host_resolver = context->host_resolver();
    session_params.cert_verifier = context->cert_verifier();
    session_params.transport_security_state =
        context->transport_security_state();
    session_params.proxy_service = context->proxy_service();
    session_params.ssl_config_service = context->ssl_config_service();
    session_params.http_auth_handler_factory =
        context->http_auth_handler_factory();
    session_params.network_delegate = context->network_delegate();
    session_params.http_server_properties =
        context->http_server_properties();
    session_params.net_log = context->net_log();
    scoped_refptr<net::HttpNetworkSession> network_session(
        new net::HttpNetworkSession(session_params));

    http_transaction_factory = new HttpNetworkLayer(network_session);
  }
  storage->set_http_transaction_factory(http_transaction_factory);

  // TODO(willchan): Support sdch.

  return context;
}

}  // namespace net
