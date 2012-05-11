// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_test_util.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_port_pair.h"
#include "net/base/server_bound_cert_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// These constants put the net::NetworkDelegate events of TestNetworkDelegate
// into an order. They are used in conjunction with
// |TestNetworkDelegate::next_states_| to check that we do not send
// events in the wrong order.
const int kStageBeforeURLRequest = 1 << 0;
const int kStageBeforeSendHeaders = 1 << 1;
const int kStageSendHeaders = 1 << 2;
const int kStageHeadersReceived = 1 << 3;
const int kStageAuthRequired = 1 << 4;
const int kStageBeforeRedirect = 1 << 5;
const int kStageResponseStarted = 1 << 6;
const int kStageCompletedSuccess = 1 << 7;
const int kStageCompletedError = 1 << 8;
const int kStageURLRequestDestroyed = 1 << 9;
const int kStageDestruction = 1 << 10;

}  // namespace

TestURLRequestContext::TestURLRequestContext()
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  context_storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    NULL));
  SetProxyDirect();
  Init();
}

TestURLRequestContext::TestURLRequestContext(bool delay_initialization)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  context_storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    NULL));
  SetProxyDirect();
  if (!delay_initialization)
    Init();
}

TestURLRequestContext::TestURLRequestContext(const std::string& proxy)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  context_storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    NULL));
  SetProxyFromString(proxy);
  Init();
}

TestURLRequestContext::TestURLRequestContext(const char* proxy)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  context_storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    NULL));
  SetProxyFromString(proxy);
  Init();
}

TestURLRequestContext::TestURLRequestContext(const std::string& proxy,
                                             net::HostResolver* host_resolver)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  context_storage_.set_host_resolver(host_resolver);
  SetProxyFromString(proxy);
  Init();
}

void TestURLRequestContext::SetProxyFromString(const std::string& proxy) {
  DCHECK(!initialized_);
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy);
  context_storage_.set_proxy_service(
      net::ProxyService::CreateFixed(proxy_config));
}

void TestURLRequestContext::SetProxyDirect() {
  DCHECK(!initialized_);
  context_storage_.set_proxy_service(net::ProxyService::CreateDirect());
}

TestURLRequestContext::~TestURLRequestContext() {
  DCHECK(initialized_);
}

void TestURLRequestContext::Init() {
  DCHECK(!initialized_);
  initialized_ = true;
  if (!cert_verifier())
    context_storage_.set_cert_verifier(net::CertVerifier::CreateDefault());
  if (!ftp_transaction_factory()) {
    context_storage_.set_ftp_transaction_factory(
        new net::FtpNetworkLayer(host_resolver()));
  }
  if (!ssl_config_service())
    context_storage_.set_ssl_config_service(new net::SSLConfigServiceDefaults);
  if (!http_auth_handler_factory()) {
    context_storage_.set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  }
  if (!http_server_properties()) {
    context_storage_.set_http_server_properties(
        new net::HttpServerPropertiesImpl);
  }
  net::HttpNetworkSession::Params params;
  params.host_resolver = host_resolver();
  params.cert_verifier = cert_verifier();
  params.proxy_service = proxy_service();
  params.ssl_config_service = ssl_config_service();
  params.http_auth_handler_factory = http_auth_handler_factory();
  params.network_delegate = network_delegate();
  params.http_server_properties = http_server_properties();

  if (!http_transaction_factory()) {
    context_storage_.set_http_transaction_factory(new net::HttpCache(
        new net::HttpNetworkSession(params),
        net::HttpCache::DefaultBackend::InMemory(0)));
  }
  // In-memory cookie store.
  if (!cookie_store())
    context_storage_.set_cookie_store(new net::CookieMonster(NULL, NULL));
  // In-memory origin bound cert service.
  if (!server_bound_cert_service()) {
    context_storage_.set_server_bound_cert_service(
        new net::ServerBoundCertService(
            new net::DefaultServerBoundCertStore(NULL),
            base::MessageLoopProxy::current()));
  }
  if (accept_language().empty())
    set_accept_language("en-us,fr");
  if (accept_charset().empty())
    set_accept_charset("iso-8859-1,*,utf-8");
  if (!job_factory())
    context_storage_.set_job_factory(new net::URLRequestJobFactory);
}


TestURLRequest::TestURLRequest(const GURL& url, Delegate* delegate)
    : net::URLRequest(url, delegate),
      context_(new TestURLRequestContext) {
  set_context(context_.get());
}

TestURLRequest::~TestURLRequest() {
  set_context(NULL);
}

TestURLRequestContextGetter::TestURLRequestContextGetter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy)
    : io_message_loop_proxy_(io_message_loop_proxy) {
  DCHECK(io_message_loop_proxy.get());
}

TestURLRequestContextGetter::~TestURLRequestContextGetter() {}

TestURLRequestContext* TestURLRequestContextGetter::GetURLRequestContext() {
  if (!context_.get())
    context_.reset(new TestURLRequestContext);
  return context_.get();
}

scoped_refptr<base::MessageLoopProxy>
TestURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return io_message_loop_proxy_;
}

TestDelegate::TestDelegate()
    : cancel_in_rr_(false),
      cancel_in_rs_(false),
      cancel_in_rd_(false),
      cancel_in_rd_pending_(false),
      quit_on_complete_(true),
      quit_on_redirect_(false),
      allow_certificate_errors_(false),
      response_started_count_(0),
      received_bytes_count_(0),
      received_redirect_count_(0),
      received_data_before_response_(false),
      request_failed_(false),
      have_certificate_errors_(false),
      certificate_errors_are_fatal_(false),
      auth_required_(false),
      buf_(new net::IOBuffer(kBufferSize)) {
}

TestDelegate::~TestDelegate() {}

void TestDelegate::OnReceivedRedirect(net::URLRequest* request,
                                      const GURL& new_url,
                                      bool* defer_redirect) {
  received_redirect_count_++;
  if (quit_on_redirect_) {
    *defer_redirect = true;
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  } else if (cancel_in_rr_) {
    request->Cancel();
  }
}

void TestDelegate::OnAuthRequired(net::URLRequest* request,
                                  net::AuthChallengeInfo* auth_info) {
  auth_required_ = true;
  if (!credentials_.Empty()) {
    request->SetAuth(credentials_);
  } else {
    request->CancelAuth();
  }
}

void TestDelegate::OnSSLCertificateError(net::URLRequest* request,
                                         const net::SSLInfo& ssl_info,
                                         bool fatal) {
  // The caller can control whether it needs all SSL requests to go through,
  // independent of any possible errors, or whether it wants SSL errors to
  // cancel the request.
  have_certificate_errors_ = true;
  certificate_errors_are_fatal_ = fatal;
  if (allow_certificate_errors_)
    request->ContinueDespiteLastError();
  else
    request->Cancel();
}

void TestDelegate::OnResponseStarted(net::URLRequest* request) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK(!request->status().is_io_pending());

  response_started_count_++;
  if (cancel_in_rs_) {
    request->Cancel();
    OnResponseCompleted(request);
  } else if (!request->status().is_success()) {
    DCHECK(request->status().status() == net::URLRequestStatus::FAILED ||
           request->status().status() == net::URLRequestStatus::CANCELED);
    request_failed_ = true;
    OnResponseCompleted(request);
  } else {
    // Initiate the first read.
    int bytes_read = 0;
    if (request->Read(buf_, kBufferSize, &bytes_read))
      OnReadCompleted(request, bytes_read);
    else if (!request->status().is_io_pending())
      OnResponseCompleted(request);
  }
}

void TestDelegate::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK(!request->status().is_io_pending());

  if (response_started_count_ == 0)
    received_data_before_response_ = true;

  if (cancel_in_rd_)
    request->Cancel();

  if (bytes_read >= 0) {
    // There is data to read.
    received_bytes_count_ += bytes_read;

    // consume the data
    data_received_.append(buf_->data(), bytes_read);
  }

  // If it was not end of stream, request to read more.
  if (request->status().is_success() && bytes_read > 0) {
    bytes_read = 0;
    while (request->Read(buf_, kBufferSize, &bytes_read)) {
      if (bytes_read > 0) {
        data_received_.append(buf_->data(), bytes_read);
        received_bytes_count_ += bytes_read;
      } else {
        break;
      }
    }
  }
  if (!request->status().is_io_pending())
    OnResponseCompleted(request);
  else if (cancel_in_rd_pending_)
    request->Cancel();
}

void TestDelegate::OnResponseCompleted(net::URLRequest* request) {
  if (quit_on_complete_)
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

TestNetworkDelegate::TestNetworkDelegate()
    : last_error_(0),
      error_count_(0),
      created_requests_(0),
      destroyed_requests_(0),
      completed_requests_(0),
      cookie_options_bit_mask_(0),
      blocked_get_cookies_count_(0),
      blocked_set_cookie_count_(0),
      set_cookie_count_(0) {
}

TestNetworkDelegate::~TestNetworkDelegate() {
  for (std::map<int, int>::iterator i = next_states_.begin();
       i != next_states_.end(); ++i) {
    event_order_[i->first] += "~TestNetworkDelegate\n";
    EXPECT_TRUE(i->second & kStageDestruction) << event_order_[i->first];
  }
}

void TestNetworkDelegate::InitRequestStatesIfNew(int request_id) {
  if (next_states_.find(request_id) == next_states_.end()) {
    next_states_[request_id] = kStageBeforeURLRequest;
    event_order_[request_id] = "";
  }
}

int TestNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url ) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeURLRequest\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeURLRequest) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeSendHeaders |
      kStageResponseStarted |  // data: URLs do not trigger sending headers
      kStageBeforeRedirect |  // a delegate can trigger a redirection
      kStageCompletedError |  // request canceled by delegate
      kStageAuthRequired;  // Auth can come next for FTP requests
  created_requests_++;
  return net::OK;
}

int TestNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeSendHeaders\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeSendHeaders) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageSendHeaders |
      kStageCompletedError;  // request canceled by delegate

  return net::OK;
}

void TestNetworkDelegate::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnSendHeaders\n";
  EXPECT_TRUE(next_states_[req_id] & kStageSendHeaders) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageHeadersReceived |
      kStageCompletedError;
}

int TestNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  int req_id = request->identifier();
  event_order_[req_id] += "OnHeadersReceived\n";
  InitRequestStatesIfNew(req_id);
  EXPECT_TRUE(next_states_[req_id] & kStageHeadersReceived) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeRedirect |
      kStageResponseStarted |
      kStageAuthRequired |
      kStageCompletedError;  // e.g. proxy resolution problem

  // Basic authentication sends a second request from the URLRequestHttpJob
  // layer before the URLRequest reports that a response has started.
  next_states_[req_id] |= kStageBeforeSendHeaders;

  return net::OK;
}

void TestNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                           const GURL& new_location) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeRedirect\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeRedirect) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeURLRequest |  // HTTP redirects trigger this.
      kStageBeforeSendHeaders |  // Redirects from the network delegate do not
                                 // trigger onBeforeURLRequest.
      kStageCompletedError;

  // A redirect can lead to a file or a data URL. In this case, we do not send
  // headers.
  next_states_[req_id] |= kStageResponseStarted;
}

void TestNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnResponseStarted\n";
  EXPECT_TRUE(next_states_[req_id] & kStageResponseStarted) <<
      event_order_[req_id];
  next_states_[req_id] = kStageCompletedSuccess | kStageCompletedError;
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    error_count_++;
    last_error_ = request->status().error();
  }
}

void TestNetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                         int bytes_read) {
}

void TestNetworkDelegate::OnCompleted(net::URLRequest* request, bool started) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnCompleted\n";
  // Expect "Success -> (next_states_ & kStageCompletedSuccess)"
  // is logically identical to
  // Expect "!(Success) || (next_states_ & kStageCompletedSuccess)"
  EXPECT_TRUE(!request->status().is_success() ||
              (next_states_[req_id] & kStageCompletedSuccess)) <<
      event_order_[req_id];
  EXPECT_TRUE(request->status().is_success() ||
              (next_states_[req_id] & kStageCompletedError)) <<
      event_order_[req_id];
  next_states_[req_id] = kStageURLRequestDestroyed;
  completed_requests_++;
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    error_count_++;
    last_error_ = request->status().error();
  }
}

void TestNetworkDelegate::OnURLRequestDestroyed(
    net::URLRequest* request) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnURLRequestDestroyed\n";
  EXPECT_TRUE(next_states_[req_id] & kStageURLRequestDestroyed) <<
      event_order_[req_id];
  next_states_[req_id] = kStageDestruction;
  destroyed_requests_++;
}

void TestNetworkDelegate::OnPACScriptError(int line_number,
                                           const string16& error) {
}

net::NetworkDelegate::AuthRequiredResponse TestNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnAuthRequired\n";
  EXPECT_TRUE(next_states_[req_id] & kStageAuthRequired) <<
      event_order_[req_id];
  next_states_[req_id] = kStageBeforeSendHeaders |
      kStageHeadersReceived |  // Request canceled by delegate simulates empty
                               // response.
      kStageResponseStarted |  // data: URLs do not trigger sending headers
      kStageBeforeRedirect;  // a delegate can trigger a redirection
  return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool TestNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                          const net::CookieList& cookie_list) {
  bool allow = true;
  if (cookie_options_bit_mask_ & NO_GET_COOKIES)
    allow = false;

  if (!allow) {
    blocked_get_cookies_count_++;
  }

  return allow;
}

bool TestNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                         const std::string& cookie_line,
                                         net::CookieOptions* options) {
  bool allow = true;
  if (cookie_options_bit_mask_ & NO_SET_COOKIE)
    allow = false;

  if (cookie_options_bit_mask_ & FORCE_SESSION)
    options->set_force_session();

  if (!allow) {
    blocked_set_cookie_count_++;
  } else {
    set_cookie_count_++;
  }

  return allow;
}

bool TestNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                          const FilePath& path) const {
  return true;
}

// static
std::string ScopedCustomUrlRequestTestHttpHost::value_("127.0.0.1");

ScopedCustomUrlRequestTestHttpHost::ScopedCustomUrlRequestTestHttpHost(
  const std::string& new_value)
    : old_value_(value_),
      new_value_(new_value) {
  value_ = new_value_;
}

ScopedCustomUrlRequestTestHttpHost::~ScopedCustomUrlRequestTestHttpHost() {
  DCHECK_EQ(value_, new_value_);
  value_ = old_value_;
}

// static
const std::string& ScopedCustomUrlRequestTestHttpHost::value() {
  return value_;
}
